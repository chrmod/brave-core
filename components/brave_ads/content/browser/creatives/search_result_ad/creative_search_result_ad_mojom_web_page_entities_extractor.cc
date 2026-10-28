/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/creative_search_result_ad_mojom_web_page_entities_extractor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/creative_search_result_ad_constants.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "components/schema_org/common/metadata.mojom.h"
#include "url/gurl.h"

namespace brave_ads {

namespace {

constexpr auto kRequiredCreativeAdPropertyNames =
    base::MakeFixedFlatSet<std::string_view>(
        base::sorted_unique,
        {kCreativeAdAdvertiserIdPropertyName, kCreativeAdCampaignIdPropertyName,
         kCreativeAdCreativeInstanceIdPropertyName,
         kCreativeAdCreativeSetIdPropertyName,
         kCreativeAdDescriptionPropertyName,
         kCreativeAdHeadlineTextPropertyName,
         kCreativeAdLandingPagePropertyName, kCreativeAdPlacementIdPropertyName,
         kCreativeAdRewardsValuePropertyName});

constexpr auto kRequiredCampaignConversionPropertyNames =
    base::MakeFixedFlatSet<std::string_view>(
        base::sorted_unique,
        {kCampaignConversionObservationWindowPropertyName,
         kCampaignConversionUrlPatternPropertyName});

constexpr auto kCampaignConversionPropertyNames =
    base::MakeFixedFlatSet<std::string_view>(
        base::sorted_unique,
        {kCampaignConversionCurrencyAttributePropertyName,
         kCampaignConversionCurrencySelectorPropertyName,
         kCampaignConversionCurrencyPropertyName,
         kCampaignConversionObservationWindowPropertyName,
         kCampaignConversionUrlPatternPropertyName,
         kCampaignConversionValueAttributePropertyName,
         kCampaignConversionValueSelectorPropertyName});

constexpr size_t kMaxConversionBasketSelectorLength = 256;
constexpr size_t kMaxConversionBasketAttributeLength = 64;

bool GetOptionalStringValue(
    const schema_org::mojom::PropertyPtr& mojom_property,
    std::string* out_value) {
  CHECK(mojom_property);
  CHECK(out_value);

  if (!mojom_property->values->is_string_values() ||
      mojom_property->values->get_string_values().size() != 1) {
    // Invalid type.
    return false;
  }

  *out_value = mojom_property->values->get_string_values()[0];

  return true;
}

bool GetRequiredStringValue(
    const schema_org::mojom::PropertyPtr& mojom_property,
    std::string* out_value) {
  CHECK(mojom_property);
  CHECK(out_value);

  if (!mojom_property->values->is_string_values() ||
      mojom_property->values->get_string_values().size() != 1) {
    // Invalid type.
    return false;
  }

  *out_value = mojom_property->values->get_string_values()[0];

  return !out_value->empty();
}

bool GetIntValue(const schema_org::mojom::PropertyPtr& mojom_property,
                 int32_t* out_value) {
  CHECK(mojom_property);
  CHECK(out_value);

  if (!mojom_property->values->is_long_values() ||
      mojom_property->values->get_long_values().size() != 1) {
    // Invalid type.
    return false;
  }

  const int64_t long_value = mojom_property->values->get_long_values()[0];
  return base::CheckedNumeric<int32_t>(long_value).AssignIfValid(out_value);
}

bool GetDoubleValue(const schema_org::mojom::PropertyPtr& mojom_property,
                    double* out_value) {
  CHECK(mojom_property);
  CHECK(out_value);

  if (!mojom_property->values->is_string_values() ||
      mojom_property->values->get_string_values().size() != 1) {
    // Invalid type.
    return false;
  }

  return base::StringToDouble(mojom_property->values->get_string_values()[0],
                              out_value);
}

bool GetUrlValue(const schema_org::mojom::PropertyPtr& mojom_property,
                 GURL* out_value) {
  CHECK(mojom_property);
  CHECK(out_value);

  if (!mojom_property->values->is_string_values() ||
      mojom_property->values->get_string_values().size() != 1) {
    // Invalid type.
    return false;
  }

  const GURL url(mojom_property->values->get_string_values()[0]);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  *out_value = url;

  return true;
}

bool ExtractCreativeAdMojomProperty(
    const schema_org::mojom::PropertyPtr& mojom_property,
    mojom::CreativeSearchResultAdInfo* mojom_creative_ad) {
  CHECK(mojom_property);
  CHECK(mojom_creative_ad);

  const std::string& property_name = mojom_property->name;
  if (property_name == kCreativeAdPlacementIdPropertyName) {
    std::string placement_id;
    if (!GetRequiredStringValue(mojom_property, &placement_id)) {
      return false;
    }

    // Escape all characters except alphanumerics and -._~ to make sure that the
    // placement id can be safely passed to JS function and can be compared with
    // an encoded placement id from search result click url.
    mojom_creative_ad->placement_id =
        base::EscapeAllExceptUnreserved(placement_id);
    return true;
  }

  if (property_name == kCreativeAdCreativeInstanceIdPropertyName) {
    return GetRequiredStringValue(mojom_property,
                                  &mojom_creative_ad->creative_instance_id);
  }

  if (property_name == kCreativeAdCreativeSetIdPropertyName) {
    return GetRequiredStringValue(mojom_property,
                                  &mojom_creative_ad->creative_set_id);
  }

  if (property_name == kCreativeAdCampaignIdPropertyName) {
    return GetRequiredStringValue(mojom_property,
                                  &mojom_creative_ad->campaign_id);
  }

  if (property_name == kCreativeAdAdvertiserIdPropertyName) {
    return GetRequiredStringValue(mojom_property,
                                  &mojom_creative_ad->advertiser_id);
  }

  if (property_name == kCreativeAdLandingPagePropertyName) {
    return GetUrlValue(mojom_property, &mojom_creative_ad->target_url);
  }

  if (property_name == kCreativeAdHeadlineTextPropertyName) {
    return GetRequiredStringValue(mojom_property,
                                  &mojom_creative_ad->headline_text);
  }

  if (property_name == kCreativeAdDescriptionPropertyName) {
    return GetRequiredStringValue(mojom_property,
                                  &mojom_creative_ad->description);
  }

  if (property_name == kCreativeAdRewardsValuePropertyName) {
    return GetDoubleValue(mojom_property, &mojom_creative_ad->value);
  }

  return false;
}

mojom::ConversionBasketFieldRuleInfo& GetOrCreateConversionBasketFieldRule(
    mojom::CampaignConversionInfo& mojom_campaign_conversion,
    bool is_currency) {
  if (!mojom_campaign_conversion.basket_rules) {
    mojom_campaign_conversion.basket_rules =
        mojom::ConversionBasketRulesInfo::New();
  }

  mojom::ConversionBasketFieldRuleInfoPtr& mojom_rule =
      is_currency ? mojom_campaign_conversion.basket_rules->currency
                  : mojom_campaign_conversion.basket_rules->value;
  if (!mojom_rule) {
    mojom_rule = mojom::ConversionBasketFieldRuleInfo::New();
  }

  return *mojom_rule;
}

bool IsValidConversionBasketFieldRule(
    const mojom::ConversionBasketFieldRuleInfoPtr& mojom_rule) {
  return mojom_rule && !mojom_rule->selector.empty() &&
         mojom_rule->selector.size() <= kMaxConversionBasketSelectorLength &&
         (!mojom_rule->attribute || (!mojom_rule->attribute->empty() &&
                                     mojom_rule->attribute->size() <=
                                         kMaxConversionBasketAttributeLength));
}

bool IsValidCurrencyCode(std::string_view currency) {
  return currency.size() == 3 && std::ranges::all_of(currency, [](char c) {
           return base::IsAsciiUpper(c);
         });
}

// Drops basket rules that are incomplete or out of bounds so that a broken
// configuration still reports the conversion, without the basket.
void MaybeDropInvalidConversionBasket(
    mojom::CampaignConversionInfo& mojom_campaign_conversion) {
  if (mojom_campaign_conversion.basket_currency &&
      !IsValidCurrencyCode(*mojom_campaign_conversion.basket_currency)) {
    VLOG(6) << "Invalid campaign conversion basket currency";
    mojom_campaign_conversion.basket_currency.reset();
  }

  mojom::ConversionBasketRulesInfoPtr& mojom_rules =
      mojom_campaign_conversion.basket_rules;
  if (!mojom_rules) {
    return;
  }

  if (mojom_rules->value &&
      !IsValidConversionBasketFieldRule(mojom_rules->value)) {
    VLOG(6) << "Invalid campaign conversion basket value rule";
    mojom_rules->value.reset();
  }

  if (mojom_rules->currency &&
      !IsValidConversionBasketFieldRule(mojom_rules->currency)) {
    VLOG(6) << "Invalid campaign conversion basket currency rule";
    mojom_rules->currency.reset();
  }

  if (mojom_campaign_conversion.basket_currency) {
    // A fixed currency takes precedence over reading it from the page.
    mojom_rules->currency.reset();
  }

  if (!mojom_rules->value) {
    // A currency without a value is meaningless.
    mojom_rules.reset();
    mojom_campaign_conversion.basket_currency.reset();
  }
}

bool ExtractCampaignConversionMojomProperty(
    const schema_org::mojom::PropertyPtr& mojom_property,
    mojom::CampaignConversionInfo* mojom_campaign_conversion) {
  CHECK(mojom_property);
  CHECK(mojom_campaign_conversion);

  const std::string& property_name = mojom_property->name;

  if (property_name == kCampaignConversionUrlPatternPropertyName) {
    std::string url_pattern;
    if (!GetOptionalStringValue(mojom_property, &url_pattern)) {
      return false;
    }
    if (!url_pattern.empty()) {
      mojom_campaign_conversion->url_pattern = std::move(url_pattern);
    }
    return true;
  }

  if (property_name == kCampaignConversionValueSelectorPropertyName ||
      property_name == kCampaignConversionCurrencySelectorPropertyName) {
    std::string selector;
    if (!GetOptionalStringValue(mojom_property, &selector)) {
      return false;
    }
    GetOrCreateConversionBasketFieldRule(
        *mojom_campaign_conversion,
        /*is_currency=*/property_name ==
            kCampaignConversionCurrencySelectorPropertyName)
        .selector = std::move(selector);
    return true;
  }

  if (property_name == kCampaignConversionValueAttributePropertyName ||
      property_name == kCampaignConversionCurrencyAttributePropertyName) {
    std::string attribute;
    if (!GetOptionalStringValue(mojom_property, &attribute)) {
      return false;
    }
    GetOrCreateConversionBasketFieldRule(
        *mojom_campaign_conversion,
        /*is_currency=*/property_name ==
            kCampaignConversionCurrencyAttributePropertyName)
        .attribute = std::move(attribute);
    return true;
  }

  if (property_name == kCampaignConversionCurrencyPropertyName) {
    std::string currency;
    if (!GetOptionalStringValue(mojom_property, &currency)) {
      return false;
    }
    if (!currency.empty()) {
      mojom_campaign_conversion->basket_currency = std::move(currency);
    }
    return true;
  }

  if (property_name == kCampaignConversionObservationWindowPropertyName) {
    int32_t observation_window;
    const bool success = GetIntValue(mojom_property, &observation_window);
    if (success) {
      mojom_campaign_conversion->observation_window =
          base::Days(observation_window);
    }

    return success;
  }

  return false;
}

mojom::CreativeSearchResultAdInfoPtr ExtractMojomEntity(
    const schema_org::mojom::EntityPtr& mojom_entity) {
  if (!mojom_entity ||
      mojom_entity->type != kCreativeSearchResultAdMojomEntityType) {
    // Unsupported type.
    return {};
  }

  mojom::CreativeSearchResultAdInfoPtr mojom_creative_ad =
      mojom::CreativeSearchResultAdInfo::New();

  mojom::CampaignConversionInfoPtr mojom_campaign_conversion =
      mojom::CampaignConversionInfo::New();

  base::flat_set<std::string_view> creative_ad_property_names;

  base::flat_set<std::string_view> campaign_conversion_property_names;

  for (const auto& mojom_property : mojom_entity->properties) {
    if (!mojom_property) {
      return {};
    }

    if (kRequiredCreativeAdPropertyNames.contains(mojom_property->name)) {
      if (!ExtractCreativeAdMojomProperty(mojom_property,
                                          mojom_creative_ad.get())) {
        VLOG(6) << "Failed to extract creative search result ad property "
                << mojom_property->name;
        return {};
      }

      creative_ad_property_names.insert(mojom_property->name);
    } else if (kCampaignConversionPropertyNames.contains(
                   mojom_property->name)) {
      if (!ExtractCampaignConversionMojomProperty(
              mojom_property, mojom_campaign_conversion.get())) {
        VLOG(6) << "Failed to extract campaign conversion property "
                << mojom_property->name;
        return {};
      }

      campaign_conversion_property_names.insert(mojom_property->name);
    }
  }

  std::vector<std::string_view> missing_creative_ad_property_names;
  std::ranges::set_difference(
      kRequiredCreativeAdPropertyNames.cbegin(),
      kRequiredCreativeAdPropertyNames.cend(),
      creative_ad_property_names.cbegin(), creative_ad_property_names.cend(),
      std::back_inserter(missing_creative_ad_property_names));

  if (!missing_creative_ad_property_names.empty()) {
    VLOG(6) << base::JoinString(missing_creative_ad_property_names, ", ")
            << " creative search result ad required properties are missing";
    return {};
  }

  if (!campaign_conversion_property_names.empty()) {
    std::vector<std::string_view> missing_campaign_conversion_property_names;
    std::ranges::set_difference(
        kRequiredCampaignConversionPropertyNames.cbegin(),
        kRequiredCampaignConversionPropertyNames.cend(),
        campaign_conversion_property_names.cbegin(),
        campaign_conversion_property_names.cend(),
        std::back_inserter(missing_campaign_conversion_property_names));

    if (missing_campaign_conversion_property_names.empty()) {
      MaybeDropInvalidConversionBasket(*mojom_campaign_conversion);
      mojom_creative_ad->campaign_conversion =
          std::move(mojom_campaign_conversion);
    } else {
      VLOG(6) << base::JoinString(missing_campaign_conversion_property_names,
                                  ", ")
              << " campaign conversion required properties are missing";
    }
  }

  if (mojom_creative_ad->placement_id.empty()) {
    // Invalid placement id.
    return {};
  }

  mojom_creative_ad->type = mojom::AdType::kSearchResultAd;

  return mojom_creative_ad;
}

std::vector<mojom::CreativeSearchResultAdInfoPtr> ExtractMojomProperty(
    const schema_org::mojom::PropertyPtr& mojom_property) {
  std::vector<mojom::CreativeSearchResultAdInfoPtr> creative_search_result_ads;

  if (!mojom_property ||
      mojom_property->name != kCreativeSearchResultAdsMojomPropertyName) {
    return {};
  }

  const schema_org::mojom::ValuesPtr& mojom_values = mojom_property->values;
  if (!mojom_values || !mojom_values->is_entity_values()) {
    return {};
  }

  for (const auto& mojom_entity : mojom_values->get_entity_values()) {
    if (auto creative_search_result_ad = ExtractMojomEntity(mojom_entity)) {
      creative_search_result_ads.push_back(
          std::move(creative_search_result_ad));
    }
  }

  return creative_search_result_ads;
}

void Log(const std::vector<mojom::CreativeSearchResultAdInfoPtr>&
             creative_search_result_ads) {
  const bool should_log = VLOG_IS_ON(6);
  if (!should_log) {
    return;
  }

  for (const auto& mojom_creative_ad : creative_search_result_ads) {
    VLOG(6) << "Creative search result ad properties:\n"
            << kCreativeAdPlacementIdPropertyName << ": "
            << mojom_creative_ad->placement_id << "\n"
            << kCreativeAdCreativeInstanceIdPropertyName << ": "
            << mojom_creative_ad->creative_instance_id << "\n"
            << kCreativeAdCreativeSetIdPropertyName << ": "
            << mojom_creative_ad->creative_set_id << "\n"
            << kCreativeAdCampaignIdPropertyName << ": "
            << mojom_creative_ad->campaign_id << "\n"
            << kCreativeAdAdvertiserIdPropertyName << ": "
            << mojom_creative_ad->advertiser_id << "\n"
            << kCreativeAdLandingPagePropertyName << ": "
            << mojom_creative_ad->target_url << "\n"
            << kCreativeAdHeadlineTextPropertyName << ": "
            << mojom_creative_ad->headline_text << "\n"
            << kCreativeAdDescriptionPropertyName << ": "
            << mojom_creative_ad->description << "\n"
            << kCreativeAdRewardsValuePropertyName << ": "
            << mojom_creative_ad->value;

    if (!mojom_creative_ad->campaign_conversion) {
      return;
    }

    const mojom::CampaignConversionInfo& campaign_conversion =
        *mojom_creative_ad->campaign_conversion;

    VLOG(6) << "Campaign conversion properties:\n"
            << kCampaignConversionObservationWindowPropertyName << ": "
            << campaign_conversion.observation_window;

    if (campaign_conversion.url_pattern) {
      VLOG(6) << kCampaignConversionUrlPatternPropertyName << ": "
              << *campaign_conversion.url_pattern;
    }

    if (campaign_conversion.basket_rules &&
        campaign_conversion.basket_rules->value) {
      VLOG(6) << kCampaignConversionValueSelectorPropertyName << ": "
              << campaign_conversion.basket_rules->value->selector;
    }
  }
}

}  // namespace

std::vector<mojom::CreativeSearchResultAdInfoPtr>
ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
    const std::vector<schema_org::mojom::EntityPtr>& mojom_entities) {
  std::vector<mojom::CreativeSearchResultAdInfoPtr> creative_search_result_ads;

  for (const auto& mojom_entity : mojom_entities) {
    if (!mojom_entity ||
        mojom_entity->type != kCreativeSearchResultAdsProductMojomEntityType) {
      continue;
    }

    for (const auto& mojom_property : mojom_entity->properties) {
      std::ranges::move(ExtractMojomProperty(mojom_property),
                        std::back_inserter(creative_search_result_ads));
    }
  }

  Log(creative_search_result_ads);

  return creative_search_result_ads;
}

}  // namespace brave_ads
