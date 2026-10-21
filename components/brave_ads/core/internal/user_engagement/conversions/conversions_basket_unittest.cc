/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <cstdint>
#include <utility>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "brave/components/brave_ads/core/internal/ad_units/test/ad_test_util.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_database_util.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_info.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/test/campaign_conversion_test_util.h"
#include "brave/components/brave_ads/core/internal/settings/test/settings_test_util.h"
#include "brave/components/brave_ads/core/internal/user_engagement/ad_events/test/ad_event_test_util.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/basket/conversion_basket_info.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/conversions.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/conversions_test_base.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/test/conversions_test_constants.h"
#include "brave/components/brave_ads/core/internal/user_engagement/conversions/test/conversions_test_util.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom.h"
#include "brave/components/brave_ads/core/public/ad_units/ad_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

// pnpm test brave_unit_tests --filter=BraveAds*

namespace brave_ads {

namespace {

constexpr int32_t kTabId = 1;

void SaveCampaignConversionWithBasket(const AdInfo& ad) {
  CampaignConversionInfo campaign_conversion =
      test::BuildCampaignConversion(ad.campaign_id,
                                       test::kMatchingUrlPattern,
                                       /*observation_window=*/base::Days(3));
  campaign_conversion.basket_value_selector = ".total";
  campaign_conversion.basket_currency = "EUR";
  database::SaveCampaignConversions({campaign_conversion});
}

}  // namespace

class BraveAdsConversionsBasketTest : public test::BraveAdsConversionsTestBase {
 protected:
  void SetUp() override {
    test::BraveAdsConversionsTestBase::SetUp();

    test::DisableBraveRewards();
  }
};

TEST_F(BraveAdsConversionsBasketTest, ConvertWithBasketReadFromPage) {
  // Arrange
  const AdInfo ad = test::BuildAd(mojom::AdType::kSearchResultAd,
                                  /*use_random_uuids=*/false);
  SaveCampaignConversionWithBasket(ad);
  test::RecordAdEvent(ad, mojom::ConfirmationType::kClicked);

  EXPECT_CALL(
      ads_client_mock_,
      ExtractConversionBasket(
          kTabId, test::BuildDefaultConversionRedirectChain().back(),
          /*mojom_rules=*/
          ::testing::Pointee(::testing::Field(
              &mojom::ConversionBasketRulesInfo::value,
              ::testing::Pointee(::testing::Field(
                  &mojom::ConversionBasketFieldRuleInfo::selector, ".total")))),
          ::testing::_))
      .WillOnce([](int32_t /*tab_id*/, const GURL& /*url*/,
                   mojom::ConversionBasketRulesInfoPtr /*mojom_rules*/,
                   ExtractConversionBasketCallback callback) {
        std::move(callback).Run(
            mojom::ConversionBasketFieldsInfo::New("€ 59,90", "USD"));
      });

  // Act & Assert
  base::RunLoop run_loop;
  VerifyOnDidConvertAdExpectation(
      ad, ConversionActionType::kClickThrough,
      ConversionBasketInfo{.value = 59.9, .currency = "EUR"},
      run_loop.QuitClosure());
  conversions_->MaybeConvert(kTabId,
                             test::BuildDefaultConversionRedirectChain());
  run_loop.Run();
}

TEST_F(BraveAdsConversionsBasketTest, ConvertWithoutBasketIfNotRead) {
  // Arrange
  const AdInfo ad = test::BuildAd(mojom::AdType::kSearchResultAd,
                                  /*use_random_uuids=*/false);
  SaveCampaignConversionWithBasket(ad);
  test::RecordAdEvent(ad, mojom::ConfirmationType::kClicked);

  EXPECT_CALL(ads_client_mock_, ExtractConversionBasket)
      .WillOnce([](int32_t /*tab_id*/, const GURL& /*url*/,
                   mojom::ConversionBasketRulesInfoPtr /*mojom_rules*/,
                   ExtractConversionBasketCallback callback) {
        std::move(callback).Run(/*mojom_fields=*/nullptr);
      });

  // Act & Assert
  base::RunLoop run_loop;
  VerifyOnDidConvertAdExpectation(ad, ConversionActionType::kClickThrough,
                                  run_loop.QuitClosure());
  conversions_->MaybeConvert(kTabId,
                             test::BuildDefaultConversionRedirectChain());
  run_loop.Run();
}

TEST_F(BraveAdsConversionsBasketTest,
       DoNotReadBasketIfCampaignDoesNotConfigureIt) {
  // Arrange
  const AdInfo ad = test::BuildAd(mojom::AdType::kSearchResultAd,
                                  /*use_random_uuids=*/false);
  test::BuildAndSaveCampaignConversion(ad.campaign_id,
                                          test::kMatchingUrlPattern,
                                          /*observation_window=*/base::Days(3));
  test::RecordAdEvent(ad, mojom::ConfirmationType::kClicked);

  EXPECT_CALL(ads_client_mock_, ExtractConversionBasket).Times(0);

  // Act & Assert
  base::RunLoop run_loop;
  VerifyOnDidConvertAdExpectation(ad, ConversionActionType::kClickThrough,
                                  run_loop.QuitClosure());
  conversions_->MaybeConvert(kTabId,
                             test::BuildDefaultConversionRedirectChain());
  run_loop.Run();
}

}  // namespace brave_ads
