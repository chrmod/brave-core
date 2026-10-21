/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_database_table_util.h"

#include <cstddef>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "brave/components/brave_ads/core/internal/common/database/database_column_util.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_info.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"

namespace brave_ads::database::table {

namespace {

// Empty strings are stored for unset optional columns.
std::optional<std::string> ColumnStringOrNullopt(
    const mojom::DBRowInfoPtr& mojom_db_row,
    size_t index) {
  std::string value = ColumnString(mojom_db_row, index);
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

CampaignConversionInfo CampaignConversionFromMojomRow(
    const mojom::DBRowInfoPtr& mojom_db_row) {
  CHECK(mojom_db_row);
  CHECK_EQ(9U, mojom_db_row->column_values_union.size());

  CampaignConversionInfo campaign_conversion;

  campaign_conversion.id = ColumnString(mojom_db_row, 0);
  campaign_conversion.url_pattern = ColumnString(mojom_db_row, 1);
  campaign_conversion.observation_window =
      base::Days(ColumnInt(mojom_db_row, 2));
  const base::Time expire_at = ColumnTime(mojom_db_row, 3);
  if (!expire_at.is_null()) {
    campaign_conversion.expire_at = expire_at;
  }
  campaign_conversion.basket_value_selector =
      ColumnStringOrNullopt(mojom_db_row, 4);
  campaign_conversion.basket_value_attribute =
      ColumnStringOrNullopt(mojom_db_row, 5);
  campaign_conversion.basket_currency =
      ColumnStringOrNullopt(mojom_db_row, 6);
  campaign_conversion.basket_currency_selector =
      ColumnStringOrNullopt(mojom_db_row, 7);
  campaign_conversion.basket_currency_attribute =
      ColumnStringOrNullopt(mojom_db_row, 8);

  return campaign_conversion;
}

}  // namespace brave_ads::database::table
