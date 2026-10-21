/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/creative_search_result_ad_mojom_web_page_entities_extractor.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/creative_search_result_ad_constants.h"
#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/creative_search_result_ad_test_constants.h"
#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/test/creative_search_result_ad_mojom_test_util.h"
#include "brave/components/brave_ads/content/browser/creatives/search_result_ad/test/creative_search_result_ad_mojom_web_page_entities_test_util.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "components/schema_org/common/metadata.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// pnpm test brave_unit_tests --filter=BraveAds*

namespace brave_ads {

namespace {

void VerifyRequiredMojomCreativeAdExpectations(
    const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad) {
  EXPECT_THAT(
      *mojom_creative_ad,
      ::testing::FieldsAre(
          mojom::AdType::kSearchResultAd, test::kCreativeAdPlacementId,
          test::kCreativeAdCreativeInstanceId, test::kCreativeAdCreativeSetId,
          test::kCreativeAdCampaignId, test::kCreativeAdAdvertiserId,
          GURL(test::kCreativeAdLandingPage), test::kCreativeAdHeadlineText,
          test::kCreativeAdDescription,
          ::testing::DoubleEq(test::kCreativeAdRewardsValue),
          /*campaign_conversion=*/::testing::_));
}

void VerifyRequiredMojomCampaignConversionExpectations(
    const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad) {
  EXPECT_THAT(
      mojom_creative_ad->campaign_conversion->url_pattern,
      ::testing::Optional(std::string(test::kCampaignConversionUrlPattern)));
  EXPECT_EQ(mojom_creative_ad->campaign_conversion->observation_window,
            test::kCampaignConversionObservationWindow);
}

// Returns the extracted campaign conversion after adding `properties`, as
// name and value pairs, to the creative search result ad entity.
mojom::CampaignConversionInfoPtr ExtractCampaignConversionWithProperties(
    const std::vector<std::pair<std::string, std::string>>& properties) {
  std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntities(
          /*excluded_property_names=*/{});
  std::vector<schema_org::mojom::PropertyPtr>& mojom_properties =
      mojom_web_page_entities[0]
          ->properties[0]
          ->values->get_entity_values()[0]
          ->properties;
  for (const auto& [name, value] : properties) {
    test::AddMojomProperty<std::string>(&mojom_properties, name, value);
  }

  std::vector<mojom::CreativeSearchResultAdInfoPtr> creative_search_result_ads =
      ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
          mojom_web_page_entities);
  CHECK_EQ(1U, creative_search_result_ads.size());
  return std::move(creative_search_result_ads[0]->campaign_conversion);
}

}  // namespace

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest, Extract) {
  const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntities(
          /*excluded_property_names=*/{});

  const std::vector<mojom::CreativeSearchResultAdInfoPtr>
      creative_search_result_ads =
          ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
              mojom_web_page_entities);
  ASSERT_THAT(creative_search_result_ads, ::testing::SizeIs(1));
  const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad =
      creative_search_result_ads[0];
  ASSERT_TRUE(mojom_creative_ad);
  ASSERT_TRUE(mojom_creative_ad->campaign_conversion);
  EXPECT_EQ(test::kCreativeAdPlacementId, mojom_creative_ad->placement_id);

  VerifyRequiredMojomCreativeAdExpectations(mojom_creative_ad);
  VerifyRequiredMojomCampaignConversionExpectations(mojom_creative_ad);
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     DoNotExtractIfEntityTypeIsUnsupported) {
  // Invalid type.
  const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntities(
          /*excluded_property_names=*/{});

  const auto& mojom_property = mojom_web_page_entities[0]->properties[0];
  const auto& mojom_entity = mojom_property->values->get_entity_values()[0];
  mojom_entity->type = "unsupported";

  const std::vector<mojom::CreativeSearchResultAdInfoPtr>
      creative_search_result_ads =
          ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
              mojom_web_page_entities);
  EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     DoNotExtractForEmptyMojomWebPageEntities) {
  {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities;

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{});
    mojom_web_page_entities[0]->type = "unsupported";

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{});
    mojom_web_page_entities[0]->properties.clear();

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{});
    const auto& mojom_property = mojom_web_page_entities[0]->properties[0];
    mojom_property->name = "unsupported";

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{});
    const auto& mojom_property = mojom_web_page_entities[0]->properties[0];
    mojom_property->values = schema_org::mojom::Values::NewEntityValues({});

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{});
    const auto& mojom_property = mojom_web_page_entities[0]->properties[0];
    mojom_property->values =
        schema_org::mojom::Values::NewStringValues({"unsupported"});

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     DoNotExtractForMissingPlacementId) {
  const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntities(
          /*excluded_property_names=*/{"data-placement-id"});

  const std::vector<mojom::CreativeSearchResultAdInfoPtr>
      creative_search_result_ads =
          ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
              mojom_web_page_entities);
  EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     ExtractWhenDataPlacementIdIncludeUnreservedCharacters) {
  const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntitiesWithProperty(
          kCreativeAdPlacementIdPropertyName,
          test::kCreativeAdPlacementIdWithUnreservedCharacters);

  const std::vector<mojom::CreativeSearchResultAdInfoPtr>
      creative_search_result_ads =
          ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
              mojom_web_page_entities);
  ASSERT_THAT(creative_search_result_ads, ::testing::SizeIs(1));
  const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad =
      creative_search_result_ads[0];
  ASSERT_TRUE(mojom_creative_ad);
  EXPECT_EQ(test::kEscapedCreativeAdPlacementIdWithUnreservedCharacters,
            mojom_creative_ad->placement_id);
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     ExtractWhenIncludesUnknownProperties) {
  const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntitiesWithProperty(
          /*name=*/"foo",
          /*value=*/"bar");

  const std::vector<mojom::CreativeSearchResultAdInfoPtr>
      creative_search_result_ads =
          ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
              mojom_web_page_entities);
  ASSERT_THAT(creative_search_result_ads, ::testing::SizeIs(1));

  const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad =
      creative_search_result_ads[0];
  ASSERT_TRUE(mojom_creative_ad);
  ASSERT_TRUE(mojom_creative_ad->campaign_conversion);
  EXPECT_EQ(test::kCreativeAdPlacementId, mojom_creative_ad->placement_id);

  VerifyRequiredMojomCreativeAdExpectations(mojom_creative_ad);
  VerifyRequiredMojomCampaignConversionExpectations(mojom_creative_ad);
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     DoNotExtractIfMissingRequiredCreativeAdProperties) {
  static constexpr auto kRequiredCreativeAdPropertyNames =
      std::to_array<std::string_view>(
          {kCreativeAdPlacementIdPropertyName,
           kCreativeAdCreativeInstanceIdPropertyName,
           kCreativeAdCreativeSetIdPropertyName,
           kCreativeAdCampaignIdPropertyName,
           kCreativeAdAdvertiserIdPropertyName,
           kCreativeAdHeadlineTextPropertyName,
           kCreativeAdDescriptionPropertyName,
           kCreativeAdLandingPagePropertyName,
           kCreativeAdRewardsValuePropertyName});

  for (auto property_name : kRequiredCreativeAdPropertyNames) {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{property_name});

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     DoNotExtractInvalidCreativeAdProperties) {
  {
    // Invalid "data-landing-page".
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntitiesWithProperty(
            kCreativeAdLandingPagePropertyName, /*value=*/"http://brave.com");

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    // Invalid "data-rewards-value".
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntitiesWithProperty(
            kCreativeAdRewardsValuePropertyName, /*value=*/"0-5");

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    // Invalid "data-conversion-observation-window-value".
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntitiesWithProperty(
            kCampaignConversionObservationWindowPropertyName, /*value=*/"1");

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }

  {
    // Invalid "data-creative-instance-id".
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{
                kCreativeAdCreativeInstanceIdPropertyName});
    const auto& mojom_property = mojom_web_page_entities[0]->properties[0];
    auto& mojom_entity = mojom_property->values->get_entity_values()[0];
    test::AddMojomProperty<int64_t>(&mojom_entity->properties,
                                    kCreativeAdCreativeInstanceIdPropertyName,
                                    /*value=*/101);

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    EXPECT_THAT(creative_search_result_ads, ::testing::IsEmpty());
  }
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     ExtractWithEmptyConversionUrlPattern) {
  // Arrange
  const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
      test::CreativeSearchResultAdMojomWebPageEntitiesWithProperty(
          kCampaignConversionUrlPatternPropertyName, /*value=*/"");

  // Act
  const std::vector<mojom::CreativeSearchResultAdInfoPtr>
      creative_search_result_ads =
          ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
              mojom_web_page_entities);

  // Assert
  ASSERT_THAT(creative_search_result_ads, ::testing::SizeIs(1));
  const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad =
      creative_search_result_ads[0];
  ASSERT_TRUE(mojom_creative_ad);
  ASSERT_TRUE(mojom_creative_ad->campaign_conversion);
  EXPECT_FALSE(mojom_creative_ad->campaign_conversion->url_pattern);
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     DoNotExtractIfMissingRequiredCampaignConversionProperties) {
  static constexpr auto kRequiredCampaignConversionPropertyNames =
      std::to_array<std::string_view>(
          {kCampaignConversionUrlPatternPropertyName,
           kCampaignConversionObservationWindowPropertyName});

  for (auto property_name : kRequiredCampaignConversionPropertyNames) {
    const std::vector<schema_org::mojom::EntityPtr> mojom_web_page_entities =
        test::CreativeSearchResultAdMojomWebPageEntities(
            /*excluded_property_names=*/{property_name});

    const std::vector<mojom::CreativeSearchResultAdInfoPtr>
        creative_search_result_ads =
            ExtractCreativeSearchResultAdsFromMojomWebPageEntities(
                mojom_web_page_entities);
    ASSERT_THAT(creative_search_result_ads, ::testing::SizeIs(1));
    const mojom::CreativeSearchResultAdInfoPtr& mojom_creative_ad =
        creative_search_result_ads[0];
    ASSERT_TRUE(mojom_creative_ad);
    EXPECT_EQ(test::kCreativeAdPlacementId, mojom_creative_ad->placement_id);

    VerifyRequiredMojomCreativeAdExpectations(mojom_creative_ad);
    EXPECT_FALSE(mojom_creative_ad->campaign_conversion);
  }
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     ExtractCampaignConversionBasket) {
  const mojom::CampaignConversionInfoPtr mojom_campaign_conversion =
      ExtractCampaignConversionWithProperties(
          {{kCampaignConversionValueSelectorPropertyName,
            "meta[name=total]"},
           {kCampaignConversionValueAttributePropertyName, "content"},
           {kCampaignConversionCurrencySelectorPropertyName, ".currency"}});
  ASSERT_TRUE(mojom_campaign_conversion);

  EXPECT_EQ(mojom::ConversionBasketRulesInfo::New(
                mojom::ConversionBasketFieldRuleInfo::New("meta[name=total]",
                                                          "content"),
                mojom::ConversionBasketFieldRuleInfo::New(
                    ".currency", /*attribute=*/std::nullopt)),
            mojom_campaign_conversion->basket_rules);
  EXPECT_FALSE(mojom_campaign_conversion->basket_currency);
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     ExtractCampaignConversionBasketWithFixedCurrency) {
  const mojom::CampaignConversionInfoPtr mojom_campaign_conversion =
      ExtractCampaignConversionWithProperties(
          {{kCampaignConversionValueSelectorPropertyName, ".total"},
           {kCampaignConversionCurrencyPropertyName, "EUR"},
           {kCampaignConversionCurrencySelectorPropertyName, ".currency"}});
  ASSERT_TRUE(mojom_campaign_conversion);

  ASSERT_TRUE(mojom_campaign_conversion->basket_rules);
  EXPECT_FALSE(mojom_campaign_conversion->basket_rules->currency);
  EXPECT_EQ("EUR", mojom_campaign_conversion->basket_currency);
}

TEST(BraveAdsCreativeSearchResultAdMojomWebPageEntitiesExtractorTest,
     ExtractCampaignConversionWithoutInvalidBasket) {
  // A currency without a value, an attribute without a selector and an invalid
  // currency code are dropped, but the conversion is kept.
  for (const auto& properties :
       std::vector<std::vector<std::pair<std::string, std::string>>>{
           {{kCampaignConversionCurrencyPropertyName, "EUR"}},
           {{kCampaignConversionValueAttributePropertyName, "content"}},
           {{kCampaignConversionValueSelectorPropertyName,
             std::string(257, 'a')}}}) {
    const mojom::CampaignConversionInfoPtr mojom_campaign_conversion =
        ExtractCampaignConversionWithProperties(properties);
    ASSERT_TRUE(mojom_campaign_conversion);

    EXPECT_FALSE(mojom_campaign_conversion->basket_rules);
    EXPECT_FALSE(mojom_campaign_conversion->basket_currency);
  }

  const mojom::CampaignConversionInfoPtr mojom_campaign_conversion =
      ExtractCampaignConversionWithProperties(
          {{kCampaignConversionValueSelectorPropertyName, ".total"},
           {kCampaignConversionCurrencyPropertyName, "euro"}});
  ASSERT_TRUE(mojom_campaign_conversion);
  EXPECT_TRUE(mojom_campaign_conversion->basket_rules);
  EXPECT_FALSE(mojom_campaign_conversion->basket_currency);
}

}  // namespace brave_ads
