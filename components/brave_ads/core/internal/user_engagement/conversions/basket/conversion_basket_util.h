/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_BASKET_CONVERSION_BASKET_UTIL_H_
#define BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_BASKET_CONVERSION_BASKET_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "brave/components/brave_ads/core/mojom/brave_ads.mojom-forward.h"

namespace brave_ads {

struct ConversionBasketInfo;
struct CampaignConversionInfo;

// Parses an amount as shown on a page, for example "€ 1.234,56" or
// "1,234.56 USD". Everything but digits, "," and "." is ignored. A "," or "."
// followed by one or two digits at the end is the decimal separator; any other
// separator groups thousands. Returns `std::nullopt` unless the amount is
// between 0 and 10,000,000.
std::optional<double> ParseConversionBasketValue(std::string_view text);

// Returns the ISO 4217 currency code in `text`, i.e., three uppercase ASCII
// letters, ignoring surrounding whitespace.
std::optional<std::string> ParseConversionBasketCurrency(std::string_view text);

// Returns the rules for reading the basket on the conversion page, or
// `nullptr` if the campaign does not report a basket.
mojom::ConversionBasketRulesInfoPtr BuildConversionBasketRules(
    const CampaignConversionInfo& campaign_conversion);

// Validates the raw strings read from the conversion page. A fixed currency
// takes precedence over the one read from the page.
ConversionBasketInfo BuildConversionBasket(
    const CampaignConversionInfo& campaign_conversion,
    const mojom::ConversionBasketFieldsInfo* mojom_fields);

}  // namespace brave_ads

#endif  // BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_BASKET_CONVERSION_BASKET_UTIL_H_
