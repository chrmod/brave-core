/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/user_engagement/conversions/basket/conversion_basket_util.h"

#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_info.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/basket/conversion_basket_info.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// npm run test -- brave_unit_tests --filter=BraveAds*

namespace brave_ads {

TEST(BraveAdsConversionBasketUtilTest, ParseConversionBasketValue) {
  // Act & Assert
  EXPECT_EQ(59.9, ParseConversionBasketValue("59.90"));
  EXPECT_EQ(59.9, ParseConversionBasketValue("59,90"));
  EXPECT_EQ(59.9, ParseConversionBasketValue("€ 59,90"));
  EXPECT_EQ(59.9, ParseConversionBasketValue("59.90 EUR"));
  EXPECT_EQ(1234.56, ParseConversionBasketValue("1,234.56"));
  EXPECT_EQ(1234.56, ParseConversionBasketValue("1.234,56 €"));
  EXPECT_EQ(1234.56, ParseConversionBasketValue("1 234,56"));
  EXPECT_EQ(1234567.0, ParseConversionBasketValue("1,234,567"));
  EXPECT_EQ(1234.0, ParseConversionBasketValue("1.234"));
  EXPECT_EQ(1.5, ParseConversionBasketValue("1.5"));
  EXPECT_EQ(0.5, ParseConversionBasketValue(".50"));
  EXPECT_EQ(42.0, ParseConversionBasketValue("$42"));
  EXPECT_EQ(59.9, ParseConversionBasketValue("59.90."));
  EXPECT_EQ(0.0, ParseConversionBasketValue("0.00"));
  EXPECT_EQ(10'000'000.0, ParseConversionBasketValue("10,000,000.00"));
}

TEST(BraveAdsConversionBasketUtilTest, DoNotParseInvalidConversionBasketValue) {
  // Act & Assert
  EXPECT_FALSE(ParseConversionBasketValue(""));
  EXPECT_FALSE(ParseConversionBasketValue("EUR"));
  EXPECT_FALSE(ParseConversionBasketValue(",."));
  EXPECT_FALSE(ParseConversionBasketValue("10,000,000.01"));
  EXPECT_FALSE(ParseConversionBasketValue("1234567890123456789"));
}

TEST(BraveAdsConversionBasketUtilTest, ParseConversionBasketCurrency) {
  // Act & Assert
  EXPECT_EQ("EUR", ParseConversionBasketCurrency("EUR"));
  EXPECT_EQ("USD", ParseConversionBasketCurrency(" USD\n"));
  EXPECT_FALSE(ParseConversionBasketCurrency("eur"));
  EXPECT_FALSE(ParseConversionBasketCurrency("€"));
  EXPECT_FALSE(ParseConversionBasketCurrency("EURO"));
  EXPECT_FALSE(ParseConversionBasketCurrency(""));
}

TEST(BraveAdsConversionBasketUtilTest, DoNotBuildConversionBasketRules) {
  // Act & Assert
  EXPECT_FALSE(BuildConversionBasketRules(CampaignConversionInfo{}));
}

TEST(BraveAdsConversionBasketUtilTest, BuildConversionBasketRules) {
  // Arrange
  CampaignConversionInfo campaign_conversion;
  campaign_conversion.basket_value_selector = "meta[name=total]";
  campaign_conversion.basket_value_attribute = "content";
  campaign_conversion.basket_currency_selector = ".currency";

  // Act
  const mojom::ConversionBasketRulesInfoPtr mojom_rules =
      BuildConversionBasketRules(campaign_conversion);

  // Assert
  ASSERT_TRUE(mojom_rules);
  EXPECT_EQ(
      mojom::ConversionBasketFieldRuleInfo::New("meta[name=total]", "content"),
      mojom_rules->value);
  EXPECT_EQ(
      mojom::ConversionBasketFieldRuleInfo::New(".currency",
                                                /*attribute=*/std::nullopt),
      mojom_rules->currency);
}

TEST(BraveAdsConversionBasketUtilTest,
     DoNotReadCurrencyFromPageIfCurrencyIsFixed) {
  // Arrange
  CampaignConversionInfo campaign_conversion;
  campaign_conversion.basket_value_selector = ".total";
  campaign_conversion.basket_currency = "EUR";
  campaign_conversion.basket_currency_selector = ".currency";

  // Act
  const mojom::ConversionBasketRulesInfoPtr mojom_rules =
      BuildConversionBasketRules(campaign_conversion);

  // Assert
  ASSERT_TRUE(mojom_rules);
  EXPECT_FALSE(mojom_rules->currency);
}

TEST(BraveAdsConversionBasketUtilTest, BuildConversionBasket) {
  // Arrange
  CampaignConversionInfo campaign_conversion;
  campaign_conversion.basket_value_selector = ".total";
  campaign_conversion.basket_currency_selector = ".currency";

  const mojom::ConversionBasketFieldsInfoPtr mojom_fields =
      mojom::ConversionBasketFieldsInfo::New("€ 59,90", "EUR");

  // Act & Assert
  EXPECT_EQ((ConversionBasketInfo{.value = 59.9, .currency = "EUR"}),
            BuildConversionBasket(campaign_conversion, mojom_fields.get()));
}

TEST(BraveAdsConversionBasketUtilTest, BuildConversionBasketWithFixedCurrency) {
  // Arrange
  CampaignConversionInfo campaign_conversion;
  campaign_conversion.basket_value_selector = ".total";
  campaign_conversion.basket_currency = "USD";

  const mojom::ConversionBasketFieldsInfoPtr mojom_fields =
      mojom::ConversionBasketFieldsInfo::New("42", "EUR");

  // Act & Assert
  EXPECT_EQ((ConversionBasketInfo{.value = 42.0, .currency = "USD"}),
            BuildConversionBasket(campaign_conversion, mojom_fields.get()));
}

TEST(BraveAdsConversionBasketUtilTest,
     BuildConversionBasketWithoutInvalidCurrency) {
  // Arrange
  CampaignConversionInfo campaign_conversion;
  campaign_conversion.basket_value_selector = ".total";
  campaign_conversion.basket_currency_selector = ".currency";

  const mojom::ConversionBasketFieldsInfoPtr mojom_fields =
      mojom::ConversionBasketFieldsInfo::New("42", "€");

  // Act & Assert
  EXPECT_EQ((ConversionBasketInfo{.value = 42.0}),
            BuildConversionBasket(campaign_conversion, mojom_fields.get()));
}

TEST(BraveAdsConversionBasketUtilTest, DoNotBuildConversionBasketWithoutValue) {
  // Arrange
  CampaignConversionInfo campaign_conversion;
  campaign_conversion.basket_value_selector = ".total";
  campaign_conversion.basket_currency = "USD";

  const mojom::ConversionBasketFieldsInfoPtr mojom_fields =
      mojom::ConversionBasketFieldsInfo::New("free", "EUR");

  // Act & Assert
  EXPECT_TRUE(BuildConversionBasket(campaign_conversion, mojom_fields.get())
                  .IsEmpty());
  EXPECT_TRUE(BuildConversionBasket(campaign_conversion,
                                    /*mojom_fields=*/nullptr)
                  .IsEmpty());
}

}  // namespace brave_ads
