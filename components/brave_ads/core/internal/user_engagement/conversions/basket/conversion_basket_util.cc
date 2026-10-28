/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/user_engagement/conversions/basket/conversion_basket_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_info.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/basket/conversion_basket_info.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"

namespace brave_ads {

namespace {

constexpr int64_t kMaxConversionBasketValueInCents = int64_t{10'000'000} * 100;

// More digits than this cannot be a valid amount, even with leading zeros, and
// would risk overflowing.
constexpr size_t kMaxConversionBasketValueIntegerDigits = 15;

constexpr std::string_view kConversionBasketValueSeparators = ",.";

bool IsConversionBasketValueSeparator(char c) {
  return c == ',' || c == '.';
}

}  // namespace

std::optional<double> ParseConversionBasketValue(std::string_view text) {
  std::string number;
  std::ranges::copy_if(text, std::back_inserter(number), [](char c) {
    return base::IsAsciiDigit(c) || IsConversionBasketValueSeparator(c);
  });

  // Drop trailing separators, for example from "59.90." at the end of a
  // sentence.
  const std::string_view trimmed_number = base::TrimString(
      number, kConversionBasketValueSeparators, base::TRIM_TRAILING);
  if (trimmed_number.empty()) {
    return std::nullopt;
  }

  std::string_view integer_part = trimmed_number;
  std::string_view fraction_part;
  if (const size_t separator_index =
          trimmed_number.find_last_of(kConversionBasketValueSeparators);
      separator_index != std::string_view::npos) {
    const size_t fraction_length = trimmed_number.size() - separator_index - 1;
    if (fraction_length == 1 || fraction_length == 2) {
      // Thousands are grouped in threes, so one or two trailing digits must be
      // the fraction.
      integer_part = trimmed_number.substr(0, separator_index);
      fraction_part = trimmed_number.substr(separator_index + 1);
    }
  }

  std::string integer_digits;
  std::ranges::copy_if(integer_part, std::back_inserter(integer_digits),
                       [](char c) { return base::IsAsciiDigit(c); });
  if (integer_digits.empty() && fraction_part.empty()) {
    return std::nullopt;
  }
  if (integer_digits.size() > kMaxConversionBasketValueIntegerDigits) {
    return std::nullopt;
  }

  int64_t integer = 0;
  if (!integer_digits.empty() &&
      !base::StringToInt64(integer_digits, &integer)) {
    return std::nullopt;
  }

  int64_t fraction = 0;
  if (!fraction_part.empty()) {
    if (!base::StringToInt64(fraction_part, &fraction)) {
      return std::nullopt;
    }
    if (fraction_part.size() == 1) {
      // "1.5" is one and a half, i.e., 50 cents.
      fraction *= 10;
    }
  }

  const int64_t cents = integer * 100 + fraction;
  if (cents > kMaxConversionBasketValueInCents) {
    return std::nullopt;
  }

  return static_cast<double>(cents) / 100;
}

std::optional<std::string> ParseConversionBasketCurrency(
    std::string_view text) {
  const std::string_view currency =
      base::TrimWhitespaceASCII(text, base::TRIM_ALL);
  if (currency.size() != 3 || !std::ranges::all_of(currency, [](char c) {
        return base::IsAsciiUpper(c);
      })) {
    return std::nullopt;
  }

  return std::string(currency);
}

mojom::ConversionBasketRulesInfoPtr BuildConversionBasketRules(
    const CampaignConversionInfo& campaign_conversion) {
  if (!campaign_conversion.basket_value_selector) {
    return nullptr;
  }

  mojom::ConversionBasketRulesInfoPtr mojom_rules =
      mojom::ConversionBasketRulesInfo::New();

  mojom_rules->value = mojom::ConversionBasketFieldRuleInfo::New(
      *campaign_conversion.basket_value_selector,
      campaign_conversion.basket_value_attribute);

  if (!campaign_conversion.basket_currency &&
      campaign_conversion.basket_currency_selector) {
    mojom_rules->currency = mojom::ConversionBasketFieldRuleInfo::New(
        *campaign_conversion.basket_currency_selector,
        campaign_conversion.basket_currency_attribute);
  }

  return mojom_rules;
}

ConversionBasketInfo BuildConversionBasket(
    const CampaignConversionInfo& campaign_conversion,
    const mojom::ConversionBasketFieldsInfo* const mojom_fields) {
  ConversionBasketInfo basket;

  if (mojom_fields && mojom_fields->value) {
    basket.value = ParseConversionBasketValue(*mojom_fields->value);
  }

  if (!basket.value) {
    // A currency without a value is meaningless.
    return {};
  }

  if (campaign_conversion.basket_currency) {
    basket.currency =
        ParseConversionBasketCurrency(*campaign_conversion.basket_currency);
  } else if (mojom_fields && mojom_fields->currency) {
    basket.currency = ParseConversionBasketCurrency(*mojom_fields->currency);
  }

  return basket;
}

}  // namespace brave_ads
