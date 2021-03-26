/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.firebase.auth;

import java.util.List;
import java.util.Map;

/** Fake FakeOAuthProvider */
public final class OAuthProvider extends FederatedAuthProvider {

  public static AuthCredential getCredential(
      String providerId, String idToken, String accessToken) {
    return new AuthCredential(providerId);
  }

  /**
   * Returns a {@link OAuthProvider.Builder} used to construct a {@link OAuthProvider} instantiated
   * with the given {@code providerId}.
   */
  public static OAuthProvider.Builder newBuilder(String providerId, FirebaseAuth firebaseAuth) {
    return new OAuthProvider.Builder();
  }

  /** Class used to create instances of {@link OAuthProvider}. */
  public static class Builder {

    /* Fake constructor */
    private Builder() {}

    /**
     * Sets the OAuth 2 scopes to be presented to the user during their sign-in flow with the
     * identity provider.
     */
    public OAuthProvider.Builder setScopes(List<String> scopes) {
      return this;
    }

    /**
     * Configures custom parameters to be passed to the identity provider during the OAuth sign-in
     * flow. Calling this method multiple times will add to the set of custom parameters being
     * passed, rather than overwriting them (as long as key values don't collide).
     *
     * @param paramKey the name of the custom parameter
     * @param paramValue the value of the custom parameter
     */
    public OAuthProvider.Builder addCustomParameter(String paramKey, String paramValue) {
      return this;
    }

    /**
     * Similar to {@link #addCustomParameter(String, String)}, this takes a Map and adds each entry
     * to the set of custom parameters to be passed. Calling this method multiple times will add to
     * the set of custom parameters being passed, rather than overwriting them (as long as key
     * values don't collide).
     *
     * @param customParameters a dictionary of custom parameter names and values to be passed to the
     *     identity provider as part of the sign-in flow.
     */
    public OAuthProvider.Builder addCustomParameters(Map<String, String> customParameters) {
      return this;
    }

    /** Returns an {@link OAuthProvider} created from this {@link Builder}. */
    public OAuthProvider build() {
      return new OAuthProvider();
    }
  }

  /**
   * Creates an {@link OAuthProvider.CredentialBuilder} for the specified provider ID.
   *
   * @throws IllegalArgumentException if {@code providerId} is null or empty
   */
  public static CredentialBuilder newCredentialBuilder(String providerId) {
    return new CredentialBuilder(providerId);
  }

  /** Builder class to initialize {@link AuthCredential}'s. */
  public static class CredentialBuilder {

    private final String providerId;

    /**
     * Internal constructor.
     */
    private CredentialBuilder(String providerId) {
      this.providerId = providerId;
    }

    /**
     * Adds an ID token to the credential being built.
     *
     * <p>If this is an OIDC ID token with a nonce field, please use {@link
     * #setIdTokenWithRawNonce(String, String)} instead.
     */
    public OAuthProvider.CredentialBuilder setIdToken(String idToken) {
      return this;
    }

    /**
     * Adds an ID token and raw nonce to the credential being built.
     *
     * <p>The raw nonce is required when an OIDC ID token with a nonce field is provided. The
     * SHA-256 hash of the raw nonce must match the nonce field in the OIDC ID token.
     */
    public OAuthProvider.CredentialBuilder setIdTokenWithRawNonce(String idToken, String rawNonce) {
      return this;
    }

    /** Adds an access token to the credential being built. */
    public OAuthProvider.CredentialBuilder setAccessToken(String accessToken) {
      return this;
    }

    /**
     * Returns the {@link AuthCredential} that this {@link CredentialBuilder} has constructed.
     *
     * @throws IllegalArgumentException if an ID token and access token were not provided.
     */
    public AuthCredential build() {
      return new AuthCredential(providerId);
    }
  }
}
