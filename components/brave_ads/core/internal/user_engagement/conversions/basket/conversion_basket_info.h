/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_BASKET_CONVERSION_BASKET_INFO_H_
#define BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_BASKET_CONVERSION_BASKET_INFO_H_

#include <optional>
#include <string>

namespace brave_ads {

// Basket read from the conversion page. Each field is set only if it passed
// validation.
struct ConversionBasketInfo final {
  bool operator==(const ConversionBasketInfo&) const = default;

  [[nodiscard]] bool IsEmpty() const { return !value && !currency; }

  std::optional<double> value;
  std::optional<std::string> currency;
};

}  // namespace brave_ads

#endif  // BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_BASKET_CONVERSION_BASKET_INFO_H_
