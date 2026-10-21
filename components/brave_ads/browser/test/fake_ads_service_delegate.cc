/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/browser/test/fake_ads_service_delegate.h"

#include <utility>

#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"

namespace brave_ads::test {

FakeAdsServiceDelegate::FakeAdsServiceDelegate() = default;

FakeAdsServiceDelegate::~FakeAdsServiceDelegate() = default;

void FakeAdsServiceDelegate::MaybeInitNotificationHelper() {}

bool FakeAdsServiceDelegate::
    CanShowSystemNotificationsWhileBrowserIsBackgrounded() {
  return false;
}

bool FakeAdsServiceDelegate::DoesSupportSystemNotifications() {
  return false;
}

bool FakeAdsServiceDelegate::CanShowNotifications() {
  return can_show_notifications_;
}

bool FakeAdsServiceDelegate::ShowOnboardingNotification() {
  return false;
}

void FakeAdsServiceDelegate::ShowScheduledCaptcha(
    const std::string& /*payment_id*/,
    const std::string& /*captcha_id*/) {}

void FakeAdsServiceDelegate::ClearScheduledCaptcha() {}

void FakeAdsServiceDelegate::SnoozeScheduledCaptcha() {}

void FakeAdsServiceDelegate::ShowNotificationAd(const std::string& id,
                                                const std::u16string& title,
                                                const std::u16string& body) {
  last_shown_notification_ad_placement_id_ = id;
  last_shown_notification_ad_title_ = title;
  last_shown_notification_ad_body_ = body;
}

void FakeAdsServiceDelegate::CloseNotificationAd(const std::string& id) {
  closed_notification_ad_ids_.push_back(id);
}

void FakeAdsServiceDelegate::OpenNewTabWithUrl(const GURL& url) {
  last_opened_url_ = url;
}

bool FakeAdsServiceDelegate::IsFullScreenMode() {
  return false;
}

void FakeAdsServiceDelegate::ExtractConversionBasket(
    int32_t /*tab_id*/,
    const GURL& /*url*/,
    mojom::ConversionBasketRulesInfoPtr /*mojom_rules*/,
    base::OnceCallback<void(mojom::ConversionBasketFieldsInfoPtr)> callback) {
  std::move(callback).Run(/*mojom_fields=*/nullptr);
}

std::string FakeAdsServiceDelegate::GetVariationsCountryCode() {
  return "US";
}

}  // namespace brave_ads::test
