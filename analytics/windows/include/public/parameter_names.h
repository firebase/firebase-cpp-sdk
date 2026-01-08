// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_PARAMETER_NAMES_H_
#define ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_PARAMETER_NAMES_H_

namespace google::analytics {

// Game achievement ID (string).
inline constexpr char kParameterAchievementId[] = "achievement_id";

// The ad format (e.g. Banner, Interstitial, Rewarded, Native, Rewarded
// Interstitial, Instream). (string).
inline constexpr char kParameterAdFormat[] = "ad_format";

// Ad Network Click ID (string). Used for network-specific click IDs which vary
// in format.
inline constexpr char kParameterAdNetworkClickId[] = "aclid";

// The ad platform (e.g. MoPub, IronSource) (string).
inline constexpr char kParameterAdPlatform[] = "ad_platform";

// The ad source (e.g. AdColony) (string).
inline constexpr char kParameterAdSource[] = "ad_source";

// The ad unit name (e.g. Banner_03) (string).
inline constexpr char kParameterAdUnitName[] = "ad_unit_name";

// A product affiliation to designate a supplying company or brick and mortar
// store location (string).
inline constexpr char kParameterAffiliation[] = "affiliation";

// Campaign custom parameter (string). Used as a method of capturing custom data
// in a campaign. Use varies by network.
inline constexpr char kParameterCP1[] = "cp1";

// The individual campaign name, slogan, promo code, etc. Some networks have
// pre-defined macro to capture campaign information, otherwise can be populated
// by developer. Highly Recommended (string).
inline constexpr char kParameterCampaign[] = "campaign";

// Campaign ID (string). Used for keyword analysis to identify a specific
// product promotion or strategic campaign. This is a required key for GA4 data
// import.
inline constexpr char kParameterCampaignId[] = "campaign_id";

// Character used in game (string).
inline constexpr char kParameterCharacter[] = "character";

// Campaign content (string).
inline constexpr char kParameterContent[] = "content";

// Type of content selected (string).
inline constexpr char kParameterContentType[] = "content_type";

// Coupon code used for a purchase (string).
inline constexpr char kParameterCoupon[] = "coupon";

// Creative Format (string). Used to identify the high-level classification of
// the type of ad served by a specific campaign.
inline constexpr char kParameterCreativeFormat[] = "creative_format";

// The name of a creative used in a promotional spot (string).
inline constexpr char kParameterCreativeName[] = "creative_name";

// The name of a creative slot (string).
inline constexpr char kParameterCreativeSlot[] = "creative_slot";

// Currency of the purchase or items associated with the event, in 3-letter
// ISO_4217 format (string).
inline constexpr char kParameterCurrency[] = "currency";

// Flight or Travel destination (string).
inline constexpr char kParameterDestination[] = "destination";

// Monetary value of discount associated with a purchase (double).
inline constexpr char kParameterDiscount[] = "discount";

// The arrival date, check-out date or rental end date for the item. This should
// be in YYYY-MM-DD format (string).
inline constexpr char kParameterEndDate[] = "end_date";

// Indicates that the associated event should either extend the current session
// or start a new session if no session was active when the event was logged.
// Specify 1 to extend the current session or to start a new session; any other
// value will not extend or start a session.
inline constexpr char kParameterExtendSession[] = "extend_session";

// Flight or Travel origin (string).
inline constexpr char kParameterFlightNumber[] = "flight_number";

// Group/clan/guild ID (string).
inline constexpr char kParameterGroupId[] = "group_id";

// Index of an item in a list (integer).
inline constexpr char kParameterIndex[] = "index";

// Item brand (string).
inline constexpr char kParameterItemBrand[] = "item_brand";

// Item category (context-specific) (string).
inline constexpr char kParameterItemCategory[] = "item_category";

// Item category (context-specific) (string).
inline constexpr char kParameterItemCategory2[] = "item_category2";

// Item category (context-specific) (string).
inline constexpr char kParameterItemCategory3[] = "item_category3";

// Item category (context-specific) (string).
inline constexpr char kParameterItemCategory4[] = "item_category4";

// Item category (context-specific) (string).
inline constexpr char kParameterItemCategory5[] = "item_category5";

// Item ID (context-specific) (string).
inline constexpr char kParameterItemId[] = "item_id";

// The ID of the list in which the item was presented to the user (string).
inline constexpr char kParameterItemListId[] = "item_list_id";

// The name of the list in which the item was presented to the user (string).
inline constexpr char kParameterItemListName[] = "item_list_name";

// Item Name (context-specific) (string).
inline constexpr char kParameterItemName[] = "item_name";

// Item variant (string).
inline constexpr char kParameterItemVariant[] = "item_variant";

// The list of items involved in the transaction expressed as `[[String: Any]]`.
inline constexpr char kParameterItems[] = "items";

// Level in game (integer).
inline constexpr char kParameterLevel[] = "level";

// Location (string). The Google <a
// href="https://developers.google.com/places/place-id">Place ID
// </a> that corresponds to the associated event. Alternatively, you can supply
// your own custom Location ID.
inline constexpr char kParameterLocation[] = "location";

// The location associated with the event. Preferred to be the Google
// <a href="https://developers.google.com/places/place-id">Place ID</a> that
// corresponds to the associated item but could be overridden to a custom
// location ID string.(string).
inline constexpr char kParameterLocationId[] = "location_id";

// Marketing Tactic (string). Used to identify the targeting criteria applied to
// a specific campaign.
inline constexpr char kParameterMarketingTactic[] = "marketing_tactic";

// The method used to perform an operation (string).
inline constexpr char kParameterMethod[] = "method";

// Number of nights staying at hotel (integer).
inline constexpr char kParameterNumberOfNights[] = "number_of_nights";

// Number of passengers traveling (integer).
inline constexpr char kParameterNumberOfPassengers[] = "number_of_passengers";

// Number of rooms for travel events (integer).
inline constexpr char kParameterNumberOfRooms[] = "number_of_rooms";

// Flight or Travel origin (string).
inline constexpr char kParameterOrigin[] = "origin";

// The chosen method of payment (string).
inline constexpr char kParameterPaymentType[] = "payment_type";

// Purchase price (double).
inline constexpr char kParameterPrice[] = "price";

// The ID of a product promotion (string).
inline constexpr char kParameterPromotionId[] = "promotion_id";

// The name of a product promotion (string).
inline constexpr char kParameterPromotionName[] = "promotion_name";

// Purchase quantity (integer).
inline constexpr char kParameterQuantity[] = "quantity";

// Score in game (integer).
inline constexpr char kParameterScore[] = "score";

// Current screen class, such as the class name of the UI view controller,
// logged with screen_view event and added to every event (string).
inline constexpr char kParameterScreenClass[] = "screen_class";

// Current screen name, such as the name of the UI view, logged with screen_view
// event and added to every event (string).
inline constexpr char kParameterScreenName[] = "screen_name";

// The search string/keywords used (string).
inline constexpr char kParameterSearchTerm[] = "search_term";

// Shipping cost associated with a transaction (double).
inline constexpr char kParameterShipping[] = "shipping";

// The shipping tier (e.g. Ground, Air, Next-day) selected for delivery of the
// purchased item (string).
inline constexpr char kParameterShippingTier[] = "shipping_tier";

// The origin of your traffic, such as an Ad network (for example, google) or
// partner (urban airship). Identify the advertiser, site, publication, etc.
// that is sending traffic to your property. Highly recommended (string).
inline constexpr char kParameterSource[] = "source";

// Source Platform (string). Used to identify the platform responsible for
// directing traffic to a given Analytics property (e.g., a buying platform
// where budgets, targeting criteria, etc. are set, a platform for managing
// organic traffic data, etc.).
inline constexpr char kParameterSourcePlatform[] = "source_platform";

// The departure date, check-in date or rental start date for the item. This
// should be in YYYY-MM-DD format (string).
inline constexpr char kParameterStartDate[] = "start_date";

// The result of an operation. Specify 1 to indicate success and 0 to indicate
// failure (integer).
inline constexpr char kParameterSuccess[] = "success";

// Tax cost associated with a transaction (double).
inline constexpr char kParameterTax[] = "tax";

// If you're manually tagging keyword campaigns, you should use utm_term to
// specify the keyword (string).
inline constexpr char kParameterTerm[] = "term";

// The unique identifier of a transaction (string).
inline constexpr char kParameterTransactionId[] = "transaction_id";

// Travel class (string).
inline constexpr char kParameterTravelClass[] = "travel_class";

// A context-specific numeric value which is accumulated automatically for each
// event type. This is a general purpose parameter that is useful for
// accumulating a key metric that pertains to an event. Examples include
// revenue, distance, time and points. Value should be specified as integer or
// double.
// Notes: Values for pre-defined currency-related events (such as @c
// kEventAddToCart) should be supplied using Double and must be accompanied by a
// @c kParameterCurrency parameter. The valid range of accumulated values is
// [-9,223,372,036,854.77, 9,223,372,036,854.77]. Supplying a non-numeric value,
// omitting the corresponding @c kParameterCurrency parameter, or supplying an
// invalid <a href="https://goo.gl/qqX3J2">currency code</a> for conversion
// events will cause that conversion to be omitted from reporting.
inline constexpr char kParameterValue[] = "value";

// The type of virtual currency being used (string).
inline constexpr char kParameterVirtualCurrencyName[] = "virtual_currency_name";

}  // namespace google::analytics

#endif  // ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_PARAMETER_NAMES_H_
