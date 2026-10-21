/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_ads/tabs/ads_tab_helper.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "brave/browser/brave_ads/ads_service_factory.h"
#include "brave/components/brave_ads/core/browser/service/ads_service.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "brave/components/brave_ads/core/public/prefs/pref_names.h"
#include "brave/components/brave_rewards/core/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif

namespace brave_ads {

struct ConversionBasketExtraction {
  GURL url;
  mojom::ConversionBasketRulesInfoPtr mojom_rules;
  ExtractConversionBasketCallback callback;
  mojom::ConversionBasketFieldsInfoPtr mojom_fields =
      mojom::ConversionBasketFieldsInfo::New();
  bool did_start = false;
  size_t retry_count = 0;
  base::OneShotTimer retry_timer;
  base::OneShotTimer timeout_timer;
};

namespace {

constexpr int kHttpClientErrorResponseStatusCodeClass = 4;
constexpr int kHttpServerErrorResponseStatusCodeClass = 5;

constexpr char16_t kDocumentBodyInnerTextJavaScript[] =
    u"document?.body?.innerText";

// Bounds how long a conversion waits for its basket fields, including waiting
// for the page to load.
constexpr base::TimeDelta kConversionBasketExtractionTimeout =
    base::Seconds(15);

// Intervals between attempts while a configured field is missing, i.e., retry
// 1, 3 and 8 seconds after the first attempt for pages that render the basket
// after load.
constexpr auto kConversionBasketExtractionRetryDelays =
    std::to_array<base::TimeDelta>(
        {base::Seconds(1), base::Seconds(2), base::Seconds(5)});

// Maximum length of a basket field, in UTF-16 code units.
constexpr size_t kMaxConversionBasketFieldLength = 64;

// Reads at most one node per field, by selector, and returns only strings.
// `$1` is the JSON encoded rules and `$2` is the maximum field length.
constexpr char kConversionBasketExtractionJavaScript[] = R"JS(
(() => {
  const rules = $1;
  const read = (rule) => {
    if (!rule) {
      return null;
    }
    let node;
    try {
      node = document.querySelector(rule.selector);
    } catch {
      return null;
    }
    if (!node) {
      return null;
    }
    const raw =
        rule.attribute ? node.getAttribute(rule.attribute) : node.textContent;
    return raw === null ? null : raw.trim().slice(0, $2);
  };
  return {value: read(rules.value), currency: read(rules.currency)};
})()
)JS";

base::flat_map</*session_id*/ int32_t, raw_ptr<AdsTabHelper>>& AdsTabHelpers() {
  static base::NoDestructor<
      base::flat_map</*session_id*/ int32_t, raw_ptr<AdsTabHelper>>>
      ads_tab_helpers;
  return *ads_tab_helpers;
}

// Another tab helper may have taken over the session id, so only remove
// `ads_tab_helper`.
void MaybeRemoveAdsTabHelper(SessionID session_id,
                             const AdsTabHelper* const ads_tab_helper) {
  auto& ads_tab_helpers = AdsTabHelpers();
  const auto iter = ads_tab_helpers.find(session_id.id());
  if (iter != ads_tab_helpers.cend() && iter->second == ads_tab_helper) {
    ads_tab_helpers.erase(iter);
  }
}

base::DictValue ConversionBasketFieldRuleToDict(
    const mojom::ConversionBasketFieldRuleInfo& mojom_rule) {
  base::DictValue dict;
  dict.Set("selector", mojom_rule.selector);
  if (mojom_rule.attribute) {
    dict.Set("attribute", *mojom_rule.attribute);
  }
  return dict;
}

std::u16string BuildConversionBasketExtractionJavaScript(
    const mojom::ConversionBasketRulesInfo& mojom_rules) {
  base::DictValue dict;
  if (mojom_rules.value) {
    dict.Set("value", ConversionBasketFieldRuleToDict(*mojom_rules.value));
  }
  if (mojom_rules.currency) {
    dict.Set("currency",
             ConversionBasketFieldRuleToDict(*mojom_rules.currency));
  }

  // The rules come from the ad, never from the page, and are JSON encoded so
  // that they are passed to the script as data.
  const std::string json = base::WriteJson(dict).value_or("{}");

  return base::UTF8ToUTF16(base::ReplaceStringPlaceholders(
      kConversionBasketExtractionJavaScript,
      {json, base::NumberToString(kMaxConversionBasketFieldLength)},
      /*offsets=*/nullptr));
}

// Returns the field only if it is a non-empty string within the length limit,
// as the renderer is not trusted to enforce it.
std::optional<std::string> MaybeGetConversionBasketField(
    const base::DictValue& dict,
    std::string_view key) {
  const std::string* const field = dict.FindString(key);
  if (!field || field->empty() ||
      base::UTF8ToUTF16(*field).size() > kMaxConversionBasketFieldLength) {
    return std::nullopt;
  }

  return *field;
}

bool HasAllConversionBasketFields(
    const ConversionBasketExtraction& extraction) {
  return (!extraction.mojom_rules->value || extraction.mojom_fields->value) &&
         (!extraction.mojom_rules->currency ||
          extraction.mojom_fields->currency);
}

// Returns 'false' if the navigation was a back/forward navigation or a reload,
// otherwise 'true'.
bool IsNewNavigation(content::NavigationHandle* const navigation_handle) {
  CHECK(navigation_handle);

  return ui::PageTransitionIsNewNavigation(
      navigation_handle->GetPageTransition());
}

// NOTE: DO NOT use this method before the navigation commit as it will return
// null. It is safe to use from `WebContentsObserver::DidFinishNavigation()`.
std::optional<int> HttpStatusCode(
    content::NavigationHandle* const navigation_handle) {
  CHECK(navigation_handle);

  if (const net::HttpResponseHeaders* const response_headers =
          navigation_handle->GetResponseHeaders()) {
    return response_headers->response_code();
  }

  return std::nullopt;
}

bool IsHttpErrorStatusCode(int http_status_code) {
  const int http_status_code_class = http_status_code / 100;
  return http_status_code_class == kHttpClientErrorResponseStatusCodeClass ||
         http_status_code_class == kHttpServerErrorResponseStatusCodeClass;
}

}  // namespace

DEFINE_USER_DATA(AdsTabHelper);

// static
AdsTabHelper* AdsTabHelper::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

AdsTabHelper::AdsTabHelper(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  Initialize();
}

// TabFeatures, and so this helper, are destroyed before the tab's contents.
AdsTabHelper::~AdsTabHelper() {
  FinishConversionBasketExtractions();

  MaybeNotifyTabdidClose();

  MaybeRemoveAdsTabHelper(session_id_, this);
}

void AdsTabHelper::SetAdsServiceForTesting(AdsService* const ads_service) {
  CHECK_IS_TEST();

  ads_service_ = ads_service;
}

void AdsTabHelper::Initialize() {
  session_id_ = sessions::SessionTabHelper::IdForTab(web_contents());
  if (!session_id_.is_valid()) {
    return;
  }

  AdsTabHelpers()[session_id_.id()] = this;

  Profile* const profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ads_service_ = AdsServiceFactory::GetForProfile(profile);
  if (!ads_service_) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // See `application_state_monitor_android.h` for Android.
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)

  MaybeSetBrowserIsActive();

  OnVisibilityChanged(web_contents()->GetVisibility());
}

// static
AdsTabHelper* AdsTabHelper::FromSessionId(SessionID session_id) {
  const auto& ads_tab_helpers = AdsTabHelpers();
  const auto iter = ads_tab_helpers.find(session_id.id());
  return iter != ads_tab_helpers.cend() ? iter->second.get() : nullptr;
}

void AdsTabHelper::ExtractConversionBasket(
    const GURL& url,
    mojom::ConversionBasketRulesInfoPtr mojom_rules,
    ExtractConversionBasketCallback callback) {
  CHECK(mojom_rules);

  if (web_contents()->GetLastCommittedURL() != url) {
    // The tab navigated away from the conversion page.
    return std::move(callback).Run(/*mojom_fields=*/nullptr);
  }

  const int32_t id = next_conversion_basket_extraction_id_++;

  auto extraction = std::make_unique<ConversionBasketExtraction>();
  extraction->url = url;
  extraction->mojom_rules = std::move(mojom_rules);
  extraction->callback = std::move(callback);
  extraction->timeout_timer.Start(
      FROM_HERE, kConversionBasketExtractionTimeout,
      base::BindOnce(&AdsTabHelper::FinishConversionBasketExtraction,
                     weak_factory_.GetWeakPtr(), id));
  conversion_basket_extractions_[id] = std::move(extraction);

  if (web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    RunConversionBasketExtraction(id);
  }
}

bool AdsTabHelper::IsNotificationAdsEnabled() const {
  const PrefService* const prefs =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();

  return prefs->GetBoolean(brave_rewards::prefs::kEnabled) &&
         prefs->GetBoolean(prefs::kNotificationsEnabled);
}

bool AdsTabHelper::IsVisible() const {
  // The web contents must be visible and the browser must be active.
  return is_web_contents_visible_ && is_browser_active_.value_or(false);
}

void AdsTabHelper::MaybeSetBrowserIsActive() {
  if (is_browser_active_.has_value() && *is_browser_active_) {
    // Already active.
    return;
  }

  is_browser_active_ = true;

  MaybeNotifyBrowserDidBecomeActive();

  // Maybe notify of tab change after the browser's active state changes because
  // `OnVisibilityChanged` can be called before `OnBrowserActivated`.
  MaybeNotifyTabDidChange();
}

void AdsTabHelper::MaybeSetBrowserIsNoLongerActive() {
  if (is_browser_active_.has_value() && !*is_browser_active_) {
    // Already inactive.
    return;
  }

  is_browser_active_ = false;

  MaybeNotifyBrowserDidResignActive();

  // Maybe notify of tab change after the browser's active state changes because
  // `OnVisibilityChanged` can be called before `OnBrowserDeactivated`.
  MaybeNotifyTabDidChange();
}

void AdsTabHelper::ProcessNavigation() {
  MaybeNotifyTabTextContentDidChange();
}

void AdsTabHelper::ResetNavigationState() {
  redirect_chain_.clear();
  redirect_chain_.shrink_to_fit();

  http_status_code_.reset();

  // Clear player tracking on navigation because all players from the previous
  // document are torn down and their IDs become invalid.
  started_media_players_.clear();
  media_players_with_audio_.clear();
}

void AdsTabHelper::MaybeNotifyBrowserDidBecomeActive() {
  if (ads_service_) {
    ads_service_->NotifyBrowserDidBecomeActive();
  }
}

void AdsTabHelper::MaybeNotifyBrowserDidResignActive() {
  if (ads_service_) {
    ads_service_->NotifyBrowserDidResignActive();
  }
}

void AdsTabHelper::MaybeNotifyUserGestureEventTriggered(
    content::NavigationHandle* const navigation_handle) {
  CHECK(navigation_handle);

  if (!ads_service_) {
    return;
  }

  if (was_restored_) {
    // Don't notify user gesture events for restored tabs.
    return;
  }

  if (!navigation_handle->HasUserGesture() &&
      navigation_handle->IsRendererInitiated()) {
    // Some browser initiated navigations return `false` for `HasUserGesture` so
    // we must also check `IsRendererInitiated`. See crbug.com/617904.
    return;
  }

  const ui::PageTransition page_transition =
      navigation_handle->GetPageTransition();
  ads_service_->NotifyUserGestureEventTriggered(page_transition);
}

void AdsTabHelper::MaybeNotifyTabDidChange() {
  if (!ads_service_) {
    return;
  }

  if (redirect_chain_.empty()) {
    // Don't notify content changes if the tab redirect chain is empty, i.e.,
    // the web contents are still loading.
    return;
  }

  ads_service_->NotifyTabDidChange(/*tab_id=*/session_id_.id(), redirect_chain_,
                                   is_new_navigation_, was_restored_,
                                   IsVisible());
}

void AdsTabHelper::MaybeNotifyTabDidLoad() {
  CHECK(http_status_code_);

  if (!ads_service_) {
    // No-op if the ads service is unavailable.
    return;
  }

  ads_service_->NotifyTabDidLoad(/*tab_id=*/session_id_.id(),
                                 *http_status_code_);
}

void AdsTabHelper::MaybeNotifyTabDidFailToLoad() {
  if (ads_service_) {
    ads_service_->NotifyTabDidFailToLoad(/*tab_id=*/session_id_.id());
  }
}

bool AdsTabHelper::ShouldNotifyTabContentDidChange() const {
  // Don't notify about content changes if the ads service is not available, the
  // tab was restored, was a previously committed navigation, the web contents
  // are still loading, or an error page was displayed. `http_status_code_` can
  // be `std::nullopt` if the navigation never finishes, which can occur if the
  // user constantly refreshes the page, or if the committed navigation was a
  // network error page.
  return ads_service_ && !was_restored_ && is_new_navigation_ &&
         !redirect_chain_.empty() && http_status_code_ &&
         !IsHttpErrorStatusCode(*http_status_code_);
}

void AdsTabHelper::MaybeNotifyTabTextContentDidChange() {
  if (!ShouldNotifyTabContentDidChange()) {
    return;
  }

  if (IsNotificationAdsEnabled()) {
    // Only utilized for text classification, which requires the user to have
    // joined Brave Rewards and notification ads to be enabled.
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
        kDocumentBodyInnerTextJavaScript,
        base::BindOnce(&AdsTabHelper::OnMaybeNotifyTabTextContentDidChange,
                       weak_factory_.GetWeakPtr(), redirect_chain_),
        ISOLATED_WORLD_ID_BRAVE_INTERNAL);
  }
}

void AdsTabHelper::OnMaybeNotifyTabTextContentDidChange(
    const std::vector<GURL>& redirect_chain,
    base::Value value) {
  if (ads_service_ && value.is_string()) {
    ads_service_->NotifyTabTextContentDidChange(/*tab_id=*/session_id_.id(),
                                                redirect_chain,
                                                /*text=*/value.GetString());
  }
}

void AdsTabHelper::MaybeRunConversionBasketExtractions() {
  std::vector<int32_t> ids;
  for (const auto& [id, extraction] : conversion_basket_extractions_) {
    if (!extraction->did_start) {
      ids.push_back(id);
    }
  }

  for (const int32_t id : ids) {
    RunConversionBasketExtraction(id);
  }
}

void AdsTabHelper::RunConversionBasketExtraction(int32_t id) {
  const auto iter = conversion_basket_extractions_.find(id);
  if (iter == conversion_basket_extractions_.cend()) {
    return;
  }
  ConversionBasketExtraction& extraction = *iter->second;

  if (web_contents()->GetLastCommittedURL() != extraction.url) {
    // The tab left the conversion page, for example by a same-document
    // navigation, so report the fields read so far rather than read on.
    return FinishConversionBasketExtraction(id);
  }

  extraction.did_start = true;

  // Main frame only, never a subframe, in an isolated world so that the page
  // cannot tamper with the script.
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      BuildConversionBasketExtractionJavaScript(*extraction.mojom_rules),
      base::BindOnce(&AdsTabHelper::OnRunConversionBasketExtraction,
                     weak_factory_.GetWeakPtr(), id),
      ISOLATED_WORLD_ID_BRAVE_INTERNAL);
}

void AdsTabHelper::OnRunConversionBasketExtraction(int32_t id,
                                                   base::Value value) {
  const auto iter = conversion_basket_extractions_.find(id);
  if (iter == conversion_basket_extractions_.cend()) {
    return;
  }
  ConversionBasketExtraction& extraction = *iter->second;

  if (const base::DictValue* const dict = value.GetIfDict()) {
    if (!extraction.mojom_fields->value) {
      extraction.mojom_fields->value =
          MaybeGetConversionBasketField(*dict, "value");
    }

    if (!extraction.mojom_fields->currency) {
      extraction.mojom_fields->currency =
          MaybeGetConversionBasketField(*dict, "currency");
    }
  }

  if (HasAllConversionBasketFields(extraction) ||
      extraction.retry_count == kConversionBasketExtractionRetryDelays.size()) {
    return FinishConversionBasketExtraction(id);
  }

  extraction.retry_timer.Start(
      FROM_HERE,
      kConversionBasketExtractionRetryDelays[extraction.retry_count++],
      base::BindOnce(&AdsTabHelper::RunConversionBasketExtraction,
                     weak_factory_.GetWeakPtr(), id));
}

void AdsTabHelper::FinishConversionBasketExtraction(int32_t id) {
  const auto iter = conversion_basket_extractions_.find(id);
  if (iter == conversion_basket_extractions_.cend()) {
    return;
  }

  std::unique_ptr<ConversionBasketExtraction> extraction =
      std::move(iter->second);
  conversion_basket_extractions_.erase(iter);

  std::move(extraction->callback).Run(std::move(extraction->mojom_fields));
}

void AdsTabHelper::FinishConversionBasketExtractions() {
  while (!conversion_basket_extractions_.empty()) {
    FinishConversionBasketExtraction(
        conversion_basket_extractions_.cbegin()->first);
  }
}

bool AdsTabHelper::IsPlayingMediaWithAudio(const content::MediaPlayerId& id) {
  return media_players_with_audio_.contains(id);
}

void AdsTabHelper::PlayerStartedPlayingWithAudio(
    const content::MediaPlayerId& id) {
  if (IsPlayingMediaWithAudio(id)) {
    return;
  }

  media_players_with_audio_.insert(id);
  if (media_players_with_audio_.size() == 1) {
    // If this is the first media player that has started playing, notify that
    // the tab has started playing media.
    MaybeNotifyTabDidStartPlayingMedia();
  }
}

void AdsTabHelper::PlayerStoppedPlayingWithAudio(
    const content::MediaPlayerId& id) {
  if (!IsPlayingMediaWithAudio(id)) {
    return;
  }

  media_players_with_audio_.erase(id);
  if (media_players_with_audio_.empty()) {
    // If this is the last media player that has stopped playing, notify that
    // the tab has stopped playing media.
    MaybeNotifyTabDidStopPlayingMedia();
  }
}

void AdsTabHelper::MaybeNotifyTabDidStartPlayingMedia() {
  if (ads_service_) {
    ads_service_->NotifyTabDidStartPlayingMedia(/*tab_id=*/session_id_.id());
  }
}

void AdsTabHelper::MaybeNotifyTabDidStopPlayingMedia() {
  if (ads_service_) {
    ads_service_->NotifyTabDidStopPlayingMedia(/*tab_id=*/session_id_.id());
  }
}

void AdsTabHelper::MaybeNotifyTabdidClose() {
  if (ads_service_) {
    ads_service_->NotifyDidCloseTab(/*tab_id=*/session_id_.id());
  }
}

void AdsTabHelper::OnDiscardContents(tabs::TabInterface* tab,
                                     content::WebContents* old_contents,
                                     content::WebContents* new_contents) {
  // The old contents are destroyed. Report the tab as closed and start over
  // with the new contents, as the ads service expects for a new tab.
  FinishConversionBasketExtractions();

  MaybeNotifyTabdidClose();

  MaybeRemoveAdsTabHelper(session_id_, this);

  weak_factory_.InvalidateWeakPtrs();
#if !BUILDFLAG(IS_ANDROID)
  browser_collection_observation_.Reset();
#endif  // !BUILDFLAG(IS_ANDROID)
  session_id_ = SessionID::InvalidValue();
  ads_service_ = nullptr;
  is_web_contents_visible_ = false;
  was_restored_ = false;
  is_new_navigation_ = false;
  redirect_chain_.clear();
  http_status_code_.reset();
  started_media_players_.clear();
  media_players_with_audio_.clear();
  is_browser_active_.reset();

  tabs::ContentsObservingTabFeature::OnDiscardContents(tab, old_contents,
                                                       new_contents);
  Initialize();
}

void AdsTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!ads_service_) {
    // No-op if the ads service is unavailable.
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Not restricted by URL scheme. Some `chrome://` pages, such as
  // `chrome://wallet`, are supported ad targets, so their navigations must
  // still reach the redirect chain that page land and conversion tracking
  // check against.

  was_restored_ =
      navigation_handle->GetRestoreType() == content::RestoreType::kRestored;

  is_new_navigation_ = IsNewNavigation(navigation_handle);

  if (!navigation_handle->IsSameDocument()) {
    // Fields read so far came from the conversion page, so report them rather
    // than read the next page.
    FinishConversionBasketExtractions();
  }

  ResetNavigationState();
}

// This method is called when a navigation in the main frame or a subframe has
// completed. It indicates that the navigation has finished, but the document
// might still be loading resources.
void AdsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!ads_service_) {
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!navigation_handle->IsSameDocument()) {
    // A request that arrived after this navigation started is still waiting
    // for the conversion page to load, which it never will, so report the
    // fields read so far rather than read the new document.
    FinishConversionBasketExtractions();
  }

  redirect_chain_ = navigation_handle->GetRedirectChain();

  http_status_code_ = HttpStatusCode(navigation_handle);

  MaybeNotifyUserGestureEventTriggered(navigation_handle);

  // Notify of tab changes after navigation completes but before notifying that
  // the tab has loaded, so that any listeners can process the tab changes
  // before the tab is considered loaded. This must also happen before
  // `MaybeNotifyTabDidFailToLoad()` below, as `TabManager` only knows about a
  // tab once it has observed a tab change for it, and looks up the tab by id
  // to include its details in the fail-to-load notification.
  MaybeNotifyTabDidChange();

  if (navigation_handle->IsErrorPage()) {
    // Reset `http_status_code_` as the page failed to load, even if response
    // headers with a non-error status code were received before the network
    // error occurred.
    http_status_code_.reset();

    // Notify the tab failed to load, instead of the tab loaded, to prevent an
    // ad landing from being incorrectly recorded for network error pages.
    MaybeNotifyTabDidFailToLoad();
    return;
  }

  if (!http_status_code_) {
    // No response headers were received, but the navigation did not commit an
    // error page, so treat it as HTTP OK.
    http_status_code_ = net::HTTP_OK;
  }

  MaybeNotifyTabDidLoad();

  if (navigation_handle->IsSameDocument() &&
      web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    // Set `was_restored_` to `false` so that listeners are notified of tab
    // changes after the tab is restored.
    was_restored_ = false;
  }
}

// This method is called when the document's onload event has fired in the
// primary main frame. This means that the document and all its subresources
// have finished loading.
void AdsTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!ads_service_) {
    return;
  }

  ProcessNavigation();

  // Conversions are checked when the navigation finishes, usually before the
  // document has loaded, so extractions requested meanwhile start now.
  MaybeRunConversionBasketExtractions();

  // Set `was_restored_` to `false` so that listeners are notified of tab
  // changes after the tab is restored.
  was_restored_ = false;
}

void AdsTabHelper::MediaStartedPlaying(const MediaPlayerInfo& video_type,
                                       const content::MediaPlayerId& id) {
  started_media_players_.insert(id);

  if (!video_type.has_audio) {
    // Media without an audio track should not suppress ads, as the user is not
    // actively engaged with audible content.
    return;
  }

  PlayerStartedPlayingWithAudio(id);
}

void AdsTabHelper::MediaStoppedPlaying(
    const MediaPlayerInfo& /*video_type*/,
    const content::MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason /*reason*/) {
  started_media_players_.erase(id);
  PlayerStoppedPlayingWithAudio(id);
}

void AdsTabHelper::MediaMutedStatusChanged(const content::MediaPlayerId& id,
                                           bool muted) {
  if (!started_media_players_.contains(id)) {
    // Only process mute transitions for players that have actually started, to
    // avoid spurious notifications from blocked autoplay or pre-start events.
    return;
  }

  if (muted) {
    PlayerStoppedPlayingWithAudio(id);
  } else {
    PlayerStartedPlayingWithAudio(id);
  }
}

void AdsTabHelper::OnVisibilityChanged(const content::Visibility visibility) {
  const bool last_is_web_contents_visible = is_web_contents_visible_;
  is_web_contents_visible_ = visibility == content::Visibility::VISIBLE;
  if (last_is_web_contents_visible != is_web_contents_visible_) {
    MaybeNotifyTabDidChange();
  }
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(https://github.com/brave/brave-browser/issues/24970): Decouple
// BrowserCollectionObserver.

void AdsTabHelper::OnBrowserActivated(BrowserWindowInterface* /*browser*/) {
  MaybeSetBrowserIsActive();
}

void AdsTabHelper::OnBrowserDeactivated(BrowserWindowInterface* /*browser*/) {
  MaybeSetBrowserIsNoLongerActive();
}
#endif

}  // namespace brave_ads
