// Copyright 2025 Google LLC

#ifndef ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_EVENT_NAMES_H_
#define ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_EVENT_NAMES_H_

// Predefined event names.
//
// An Event is an important occurrence in your app that you want to measure. You
// can report up to 500 different types of Events per app and you can associate
// up to 25 unique parameters with each Event type. Some common events are
// suggested below, but you may also choose to specify custom Event types that
// are associated with your specific app. Each event type is identified by a
// unique name. Event names can be up to 40 characters long, may only contain
// alphanumeric characters and underscores ("_"), and must start with an
// alphabetic character. The "firebase_", "google_", and "ga_" prefixes are
// reserved and should not be used.

namespace google::analytics {

// Ad Impression event. This event signifies when a user sees an ad impression.
// Note: If you supply the @c kParameterValue parameter, you must also supply
// the @c kParameterCurrency parameter so that revenue metrics can be computed
// accurately. Params:
//
// <ul>
//     <li>@c kParameterAdPlatform (string) (optional)</li>
//     <li>@c kParameterAdFormat (string) (optional)</li>
//     <li>@c kParameterAdSource (string) (optional)</li>
//     <li>@c kParameterAdUnitName (string) (optional)</li>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventPublicAdImpression[] = "ad_impression";

// Add Payment Info event. This event signifies that a user has submitted their
// payment information. Note: If you supply the @c kParameterValue parameter,
// you must also supply the @c kParameterCurrency parameter so that revenue
// metrics can be computed accurately. Params:
//
// <ul>
//     <li>@c kParameterCoupon (string) (optional)</li>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterPaymentType (string) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventAddPaymentInfo[] = "add_payment_info";

// Add Shipping Info event. This event signifies that a user has submitted their
// shipping information. Note: If you supply the @c kParameterValue parameter,
// you must also supply the @c kParameterCurrency parameter so that revenue
// metrics can be computed accurately. Params:
//
// <ul>
//     <li>@c kParameterCoupon (string) (optional)</li>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterShippingTier (string) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventAddShippingInfo[] = "add_shipping_info";

// Add to Cart event. This event signifies that an item was added to a cart for
// purchase. Add this event to a funnel with @c kEventPurchase to gauge the
// effectiveness of your checkout process. Note: If you supply the
// @c kParameterValue parameter, you must also supply the @c kParameterCurrency
// parameter so that revenue metrics can be computed accurately. Params:
//
// <ul>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventAddToCart[] = "add_to_cart";

// Add to Wishlist event. This event signifies that an item was added to a
// wishlist. Use this event to identify popular gift items. Note: If you supply
// the @c kParameterValue parameter, you must also supply the @c
// kParameterCurrency parameter so that revenue metrics can be computed
// accurately. Params:
//
// <ul>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventAddToWishlist[] = "add_to_wishlist";

// App Open event. By logging this event when an App becomes active, developers
// can understand how often users leave and return during the course of a
// Session. Although Sessions are automatically reported, this event can provide
// further clarification around the continuous engagement of app-users.
inline constexpr char kEventAppOpen[] = "app_open";

// E-Commerce Begin Checkout event. This event signifies that a user has begun
// the process of checking out. Add this event to a funnel with your @c
// kEventPurchase event to gauge the effectiveness of your checkout process.
// Note: If you supply the @c kParameterValue parameter, you must also supply
// the @c kParameterCurrency parameter so that revenue metrics can be computed
// accurately. Params:
//
// <ul>
//     <li>@c kParameterCoupon (string) (optional)</li>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventBeginCheckout[] = "begin_checkout";

// Campaign Detail event. Log this event to supply the referral details of a
// re-engagement campaign. Note: you must supply at least one of the required
// parameters kParameterSource, kParameterMedium or kParameterCampaign. Params:
//
// <ul>
//     <li>@c kParameterSource (string)</li>
//     <li>@c kParameterMedium (string)</li>
//     <li>@c kParameterCampaign (string)</li>
//     <li>@c kParameterTerm (string) (optional)</li>
//     <li>@c kParameterContent (string) (optional)</li>
//     <li>@c kParameterAdNetworkClickId (string) (optional)</li>
//     <li>@c kParameterCP1 (string) (optional)</li>
// </ul>
inline constexpr char kEventCampaignDetails[] = "campaign_details";

// Earn Virtual Currency event. This event tracks the awarding of virtual
// currency in your app. Log this along with @c kEventSpendVirtualCurrency to
// better understand your virtual economy. Params:
//
// <ul>
//     <li>@c kParameterVirtualCurrencyName (string)</li>
//     <li>@c kParameterValue (signed 64-bit integer or double)</li>
// </ul>
inline constexpr char kEventEarnVirtualCurrency[] = "earn_virtual_currency";

// Generate Lead event. Log this event when a lead has been generated in the app
// to understand the efficacy of your install and re-engagement campaigns. Note:
// If you supply the
// @c kParameterValue parameter, you must also supply the @c kParameterCurrency
// parameter so that revenue metrics can be computed accurately. Params:
//
// <ul>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventGenerateLead[] = "generate_lead";

// Join Group event. Log this event when a user joins a group such as a guild,
// team or family. Use this event to analyze how popular certain groups or
// social features are in your app. Params:
//
// <ul>
//     <li>@c kParameterGroupId (string)</li>
// </ul>
inline constexpr char kEventJoinGroup[] = "join_group";

// Level Start event. Log this event when the user starts a level. Params:
//
// <ul>
//     <li>@c kParameterLevelName (string)</li>
//     <li>@c kParameterSuccess (string)</li>
// </ul>
inline constexpr char kEventLevelEnd[] = "level_end";

// Level Up event. Log this event when the user finishes a level. Params:
//
// <ul>
//     <li>@c kParameterLevelName (string)</li>
//     <li>@c kParameterSuccess (string)</li>
// </ul>
inline constexpr char kEventLevelStart[] = "level_start";

// Level Up event. This event signifies that a player has leveled up in your
// gaming app. It can help you gauge the level distribution of your userbase
// and help you identify certain levels that are difficult to pass. Params:
//
// <ul>
//     <li>@c kParameterLevel (signed 64-bit integer)</li>
//     <li>@c kParameterCharacter (string) (optional)</li>
// </ul>
inline constexpr char kEventLevelUp[] = "level_up";

// Login event. Apps with a login feature can report this event to signify that
// a user has logged in.
inline constexpr char kEventLogin[] = "login";

// Post Score event. Log this event when the user posts a score in your gaming
// app. This event can help you understand how users are actually performing in
// your game and it can help you correlate high scores with certain audiences or
// behaviors. Params:
//
// <ul>
//     <li>@c kParameterScore (signed 64-bit integer)</li>
//     <li>@c kParameterLevel (signed 64-bit integer) (optional)</li>
//     <li>@c kParameterCharacter (string) (optional)</li>
// </ul
inline constexpr char kEventPostScore[] = "post_score";

// E-Commerce Purchase event. This event signifies that an item(s) was
// purchased by a user. Note: This is different from the in-app purchase event,
// which is reported automatically for App Store-based apps. Note: If you supply
// the @c kParameterValue parameter, you must also supply the
// @c kParameterCurrency parameter so that revenue metrics can be computed
// accurately. Params:
//
// <ul>
//     <li>@c kParameterAffiliation (string) (optional)</li>
//     <li>@c kParameterCoupon (string) (optional)</li>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterShipping (double) (optional)</li>
//     <li>@c kParameterTax (double) (optional)</li>
//     <li>@c kParameterTransactionId (string) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventPurchase[] = "purchase";

// E-Commerce Refund event. This event signifies that a refund was issued.
// Note: If you supply the @c kParameterValue parameter, you must also supply
// the @c kParameterCurrency parameter so that revenue metrics can be computed
// accurately. Params:
//
// <ul>
//     <li>@c kParameterAffiliation (string) (optional)</li>
//     <li>@c kParameterCoupon (string) (optional)</li>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterShipping (double) (optional)</li>
//     <li>@c kParameterTax (double) (optional)</li>
//     <li>@c kParameterTransactionId (string) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventRefund[] = "refund";

// E-Commerce Remove from Cart event. This event signifies that an item(s) was
// removed from a cart. Note: If you supply the @c kParameterValue parameter,
// you must also supply the @c kParameterCurrency parameter so that revenue
// metrics can be computed accurately. Params:
//
// <ul>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventRemoveFromCart[] = "remove_from_cart";

// Screen View event. This event signifies that a screen in your app has
// appeared. Use this event to contextualize Events that occur on a specific
// screen. Note: The @c kParameterScreenName parameter is optional, and the @c
// kParameterScreenClass parameter is required. If the @c kParameterScreenClass
// is not provided, or if there are extra parameters, the call to log this event
// will be ignored. Params:
//
// <ul>
//     <li>@c kParameterScreenClass (string) (required)</li>
//     <li>@c kParameterScreenName (string) (optional)</li>
// </ul>
inline constexpr char kEventScreenView[] = "screen_view";

// Search event. Apps that support search features can use this event to
// contextualize search operations by supplying the appropriate, corresponding
// parameters. This event can help you identify the most popular content in your
// app. Params:
//
// <ul>
//     <li>@c kParameterSearchTerm (string)</li>
//     <li>@c kParameterStartDate (string) (optional)</li>
//     <li>@c kParameterEndDate (string) (optional)</li>
//     <li>@c kParameterNumberOfNights (signed 64-bit integer)
//         (optional) for hotel bookings</li>
//     <li>@c kParameterNumberOfRooms (signed 64-bit integer)
//         (optional) for hotel bookings</li>
//     <li>@c kParameterNumberOfPassengers (signed 64-bit integer)
//         (optional) for travel bookings</li>
//     <li>@c kParameterOrigin (string) (optional)</li>
//     <li>@c kParameterDestination (string) (optional)</li>
//     <li>@c kParameterTravelClass (string) (optional) for travel bookings</li>
// </ul>
inline constexpr char kEventSearch[] = "search";

// Select Content event. This general purpose event signifies that a user has
// selected some content of a certain type in an app. The content can be any
// object in your app. This event can help you identify popular content and
// categories of content in your app. Params:
//
// <ul>
//     <li>@c kParameterContentType (string)</li>
//     <li>@c kParameterItemId (string)</li>
// </ul>
inline constexpr char kEventSelectContent[] = "select_content";

// Select Item event. This event signifies that an item was selected by a user
// from a list. Use the appropriate parameters to contextualize the event. Use
// this event to discover the most popular items selected. Params:
//
// <ul>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterItemListId (string) (optional)</li>
//     <li>@c kParameterItemListName (string) (optional)</li>
// </ul>
inline constexpr char kEventSelectItem[] = "select_item";

// Select Promotion event. This event signifies that a user has selected a
// promotion offer. Use the appropriate parameters to contextualize the event,
// such as the item(s) for which the promotion applies. Params:
//
// <ul>
//     <li>@c kParameterCreativeName (string) (optional)</li>
//     <li>@c kParameterCreativeSlot (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterLocationId (string) (optional)</li>
//     <li>@c kParameterPromotionId (string) (optional)</li>
//     <li>@c kParameterPromotionName (string) (optional)</li>
// </ul>
inline constexpr char kEventSelectPromotion[] = "select_promotion";

// Share event. Apps with social features can log the Share event to identify
// the most viral content. Params:
//
// <ul>
//     <li>@c kParameterContentType (string)</li>
//     <li>@c kParameterItemId (string)</li>
// </ul>
inline constexpr char kEventShare[] = "share";

// Sign Up event. This event indicates that a user has signed up for an account
// in your app. The parameter signifies the method by which the user signed up.
// Use this event to understand the different behaviors between logged in and
// logged out users. Params:
//
// <ul>
//     <li>@c kParameterSignUpMethod (string)</li>
// </ul>
inline constexpr char kEventSignUp[] = "sign_up";

// Spend Virtual Currency event. This event tracks the sale of virtual goods in
// your app and can help you identify which virtual goods are the most popular
// objects of purchase. Params:
//
// <ul>
//     <li>@c kParameterItemName (string)</li>
//     <li>@c kParameterVirtualCurrencyName (string)</li>
//     <li>@c kParameterValue (signed 64-bit integer or double)</li>
// </ul>
inline constexpr char kEventSpendVirtualCurrency[] = "spend_virtual_currency";

// Tutorial Begin event. This event signifies the start of the on-boarding
// process in your app. Use this in a funnel with kEventTutorialComplete to
// understand how many users complete this process and move on to the full app
// experience.
inline constexpr char kEventTutorialBegin[] = "tutorial_begin";

// Tutorial End event. Use this event to signify the user's completion of your
// app's on-boarding process. Add this to a funnel with kEventTutorialBegin to
// gauge the completion rate of your on-boarding process.
inline constexpr char kEventTutorialComplete[] = "tutorial_complete";

// Unlock Achievement event. Log this event when the user has unlocked an
// achievement in your game. Since achievements generally represent the breadth
// of a gaming experience, this event can help you understand how many users
// are experiencing all that your game has to offer. Params:
//
// <ul>
//     <li>@c kParameterAchievementId (string)</li>
// </ul>
inline constexpr char kEventUnlockAchievement[] = "unlock_achievement";

// E-commerce View Cart event. This event signifies that a user has viewed
// their cart. Use this to analyze your purchase funnel. Note: If you supply
// the @c kParameterValue parameter, you must also supply the
// @c kParameterCurrency parameter so that revenue metrics can be computed
// accurately. Params:
//
// <ul>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventViewCart[] = "view_cart";

// View Item event. This event signifies that a user has viewed an item. Use
// the appropriate parameters to contextualize the event. Use this event to
// discover the most popular items viewed in your app. Note: If you supply the
// @c kParameterValue parameter, you must also supply the @c kParameterCurrency
// parameter so that revenue metrics can be computed accurately. Params:
//
// <ul>
//     <li>@c kParameterCurrency (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterValue (double) (optional)</li>
// </ul>
inline constexpr char kEventViewItem[] = "view_item";

// View Item List event. Log this event when a user sees a list of items or
// offerings. Params:
//
// <ul>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterItemListId (string) (optional)</li>
//     <li>@c kParameterItemListName (string) (optional)</li>
// </ul>
inline constexpr char kEventViewItemList[] = "view_item_list";

// View Promotion event. This event signifies that a promotion was shown to a
// user. Add this event to a funnel with the @c kEventAddToCart and
// @c kEventPurchase to gauge your conversion process. Params:
//
// <ul>
//     <li>@c kParameterCreativeName (string) (optional)</li>
//     <li>@c kParameterCreativeSlot (string) (optional)</li>
//     <li>@c kParameterItems (array) (optional)</li>
//     <li>@c kParameterLocationId (string) (optional)</li>
//     <li>@c kParameterPromotionId (string) (optional)</li>
//     <li>@c kParameterPromotionName (string) (optional)</li>
// </ul>
inline constexpr char kEventViewPromotion[] = "view_promotion";

// View Search Results event. Log this event when the user has been presented
// with the results of a search. Params:
//
// <ul>
//     <li>@c kParameterSearchTerm (string)</li>
// </ul>
inline constexpr char kEventViewSearchResults[] = "view_search_results";

}  // namespace google::analytics

#endif  // ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_EVENT_NAMES_H_
