/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_CONVERSIONS_H_
#define BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_CONVERSIONS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_database_table.h"
#include "brave/components/brave_ads/core/internal/creatives/conversions/campaign_conversion_info.h"
#include "brave/components/brave_ads/core/internal/tabs/tab_manager_observer.h"
#include "brave/components/brave_ads/core/internal/user_engagement/ad_events/ad_event_info.h"
#include "brave/components/brave_ads/core/internal/user_engagement/ad_events/ad_events_database_table.h"
#include "brave/components/brave_ads/core/mojom/brave_ads.mojom-forward.h"

class GURL;

namespace brave_ads {

class ConversionsObserver;
class TabManager;
struct ConversionBasketInfo;
struct ConversionInfo;

class Conversions final : public TabManagerObserver {
 public:
  Conversions();

  Conversions(const Conversions&) = delete;
  Conversions& operator=(const Conversions&) = delete;

  ~Conversions() override;

  void AddObserver(ConversionsObserver* observer);
  void RemoveObserver(ConversionsObserver* observer);

  // Examine potential view-through or click-through conversions by matching
  // URL patterns in the redirect chain of the tab for `tab_id` against known
  // campaign conversions.
  void MaybeConvert(int32_t tab_id, const std::vector<GURL>& redirect_chain);

 private:
  void GetCampaignConversions(int32_t tab_id,
                              const std::vector<GURL>& redirect_chain);
  void GetCampaignConversionsCallback(
      int32_t tab_id,
      const std::vector<GURL>& redirect_chain,
      bool success,
      const CampaignConversionList& campaign_conversions);

  void GetAdEvents(int32_t tab_id,
                   const std::vector<GURL>& redirect_chain,
                   const CampaignConversionList& campaign_conversions);
  void GetAdEventsCallback(int32_t tab_id,
                           const std::vector<GURL>& redirect_chain,
                           const CampaignConversionList& campaign_conversions,
                           bool success,
                           const AdEventList& ad_events);

  void CheckForConversions(int32_t tab_id,
                           const std::vector<GURL>& redirect_chain,
                           const CampaignConversionList& campaign_conversions,
                           const AdEventList& ad_events);

  // Reads the basket from the conversion page first if the campaign configures
  // where to find it.
  void Convert(int32_t tab_id,
               const GURL& url,
               const AdEventInfo& ad_event,
               const CampaignConversionInfo& campaign_conversion);
  void ExtractConversionBasketCallback(
      const AdEventInfo& ad_event,
      const CampaignConversionInfo& campaign_conversion,
      mojom::ConversionBasketFieldsInfoPtr mojom_fields);
  void RecordConversion(const AdEventInfo& ad_event,
                        const ConversionBasketInfo& basket);
  void RecordConversionCallback(const AdEventInfo& ad_event,
                                const ConversionBasketInfo& basket,
                                bool success);

  void NotifyDidConvertAd(const ConversionInfo& conversion);
  void NotifyFailedToConvertAd(const std::string& creative_instance_id);

  // TabManagerObserver:
  void OnTabDidChange(const TabInfo& tab) override;

  base::ObserverList<ConversionsObserver> observers_;

  const database::table::CampaignConversions
      campaign_conversions_database_table_;

  const database::table::AdEvents ad_events_database_table_;

  base::ScopedObservation<TabManager, TabManagerObserver>
      tab_manager_observation_{this};

  base::WeakPtrFactory<Conversions> weak_factory_{this};
};

}  // namespace brave_ads

#endif  // BRAVE_COMPONENTS_BRAVE_ADS_CORE_INTERNAL_USER_ENGAGEMENT_CONVERSIONS_CONVERSIONS_H_
