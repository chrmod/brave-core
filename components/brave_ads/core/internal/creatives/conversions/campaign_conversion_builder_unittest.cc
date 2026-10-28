/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_builder.h"

#include "brave/components/brave_ads/core/internal/ad_units/test/ad_test_constants.h"
#include "brave/components/brave_ads/core/internal/common/test/test_base.h"
#include "brave/components/brave_ads/core/internal/common/test/time_test_util.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_info.h"
#include "brave/components/brave_ads/core/internal/creatives/search_result_ads/test/creative_search_result_ad_test_util.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"

// pnpm test brave_unit_tests --filter=BraveAds*

namespace brave_ads {

class BraveAdsCampaignConversionBuilderTest : public test::TestBase {};

TEST_F(BraveAdsCampaignConversionBuilderTest,
       FromMojomMaybeBuildCampaignConversion) {
  // Arrange
  const mojom::CreativeSearchResultAdInfoPtr mojom_creative_ad =
      test::BuildCreativeSearchResultAdWithConversion(
          /*use_random_uuids=*/false);

  // Act
  std::optional<CampaignConversionInfo> campaign_conversion =
      FromMojomMaybeBuildCampaignConversion(mojom_creative_ad);
  ASSERT_TRUE(campaign_conversion);

  // Assert
  EXPECT_THAT(*campaign_conversion,
              ::testing::FieldsAre(test::kCampaignId,
                                   /*url_pattern*/ "https://brave.com/*",
                                   /*observation_window*/ base::Days(3),
                                   /*expire_at*/ test::Now() + base::Days(3),
                                   /*basket_value_selector*/ std::nullopt,
                                   /*basket_value_attribute*/ std::nullopt,
                                   /*basket_currency*/ std::nullopt,
                                   /*basket_currency_selector*/ std::nullopt,
                                   /*basket_currency_attribute*/ std::nullopt));
}

TEST_F(BraveAdsCampaignConversionBuilderTest,
       BuildCampaignConversionWithBasket) {
  // Arrange
  const mojom::CreativeSearchResultAdInfoPtr mojom_creative_ad =
      test::BuildCreativeSearchResultAdWithConversion(
          /*use_random_uuids=*/false);
  mojom_creative_ad->campaign_conversion->basket_rules =
      mojom::ConversionBasketRulesInfo::New(
          mojom::ConversionBasketFieldRuleInfo::New(".total",
                                                    /*attribute=*/std::nullopt),
          /*currency=*/nullptr);
  mojom_creative_ad->campaign_conversion->basket_currency = "EUR";

  // Act
  std::optional<CampaignConversionInfo> campaign_conversion =
      FromMojomMaybeBuildCampaignConversion(mojom_creative_ad);
  ASSERT_TRUE(campaign_conversion);

  // Assert
  EXPECT_EQ(".total", campaign_conversion->basket_value_selector);
  EXPECT_FALSE(campaign_conversion->basket_value_attribute);
  EXPECT_EQ("EUR", campaign_conversion->basket_currency);
  EXPECT_FALSE(campaign_conversion->basket_currency_selector);
}

TEST_F(BraveAdsCampaignConversionBuilderTest,
       DoNotBuildCampaignConversionIfUrlPatternIsAbsent) {
  // Arrange
  const mojom::CreativeSearchResultAdInfoPtr mojom_creative_ad =
      test::BuildCreativeSearchResultAdWithConversion(
          /*use_random_uuids=*/false);
  mojom_creative_ad->campaign_conversion->url_pattern.reset();

  // Act & Assert
  EXPECT_FALSE(FromMojomMaybeBuildCampaignConversion(mojom_creative_ad));
}

TEST_F(BraveAdsCampaignConversionBuilderTest,
       DoNotBuildCampaignConversionIfAdDoesNotSupportConversions) {
  // Arrange
  const mojom::CreativeSearchResultAdInfoPtr mojom_creative_ad =
      test::BuildCreativeSearchResultAd(/*use_random_uuids=*/false);

  // Act & Assert
  EXPECT_FALSE(FromMojomMaybeBuildCampaignConversion(mojom_creative_ad));
}

}  // namespace brave_ads
