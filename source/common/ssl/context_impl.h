#pragma once

#include "context_impl.h"
#include "openssl.h"

#include "envoy/runtime/runtime.h"
#include "envoy/ssl/context.h"
#include "envoy/ssl/context_config.h"
#include "envoy/stats/stats.h"
#include "envoy/stats/stats_macros.h"

namespace Ssl {

// clang-format off
#define ALL_SSL_STATS(COUNTER, GAUGE, TIMER)                                                       \
  COUNTER(connection_error)                                                                        \
  COUNTER(handshake)                                                                               \
  COUNTER(no_certificate)                                                                          \
  COUNTER(fail_verify_san)                                                                         \
  COUNTER(fail_verify_cert_hash)
// clang-format on

/**
 * Wrapper struct for SSL stats. @see stats_macros.h
 */
struct SslStats {
  ALL_SSL_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT, GENERATE_TIMER_STRUCT)
};

class ContextImpl : public virtual Context {
public:
  virtual SSL* newSsl() const;

  /**
   * Performs all configured cert verifications on the connection
   * @param ssl the connection to verify
   * @return true if all configured cert verifications succeed
   */
  bool verifyPeer(SSL* ssl) const;

  /**
   * Determines whether the given name matches 'pattern' which may optionally begin with a wildcard.
   * NOTE:  public for testing
   * @param san the subjectAltName to match
   * @param pattern the pattern to match against (*.example.com)
   * @return true if the san matches pattern
   */
  static bool sanMatch(const std::string& san, const char* pattern);

  SslStats& stats() { return stats_; }

  // Ssl::Context
  size_t daysUntilFirstCertExpires() override;
  std::string getCaCertInformation() override;
  std::string getCertChainInformation() override;

protected:
  ContextImpl(const std::string& name, Stats::Store& stats, ContextConfig& config);

  /**
   * Specifies the context for which the session can be reused.  Any data is acceptable here.
   * @see SSL_CTX_set_session_id_ctx
   */
  static const unsigned char SERVER_SESSION_ID_CONTEXT;

  typedef CSmartPtr<SSL_CTX, SSL_CTX_free> SslCtxPtr;
  typedef CSmartPtr<SSL, SSL_free> SslConPtr;

  /**
   * Performs subjectAltName verification
   * @param ssl the certificate to verify
   * @param subject_alt_name the configured subject_alt_name to match
   * @return true if the verification succeeds
   */
  static bool verifySubjectAltName(X509* cert, const std::string& subject_alt_name);

  /**
   * Verifies certificate hash for pinning. The hash is the SHA-256 has of the DER encoding of the
   * certificate.
   *
   * The hash can be computed using 'openssl x509 -noout -fingerprint -sha256 -in cert.pem'
   *
   * @param ssl the certificate to verify
   * @param certificate_hash the configured certificate hash to match
   * @return true if the verification succeeds
   */
  static bool verifyCertificateHash(X509* cert, const std::vector<uint8_t>& certificate_hash);

  std::vector<uint8_t> parseAlpnProtocols(const std::string& alpn_protocols);
  static SslStats generateStats(const std::string& prefix, Stats::Store& store);
  int32_t getDaysUntilExpiration(const X509* cert);
  X509Ptr loadCert(const std::string& cert_file);
  static std::string getSerialNumber(X509* cert);
  std::string getCaFileName() { return ca_file_path_; };
  std::string getCertChainFileName() { return cert_chain_file_path_; };

  SslCtxPtr ctx_;
  std::string verify_subject_alt_name_;
  std::vector<uint8_t> verify_certificate_hash_;
  Stats::Store& store_;
  const std::string stats_prefix_;
  SslStats stats_;
  std::vector<uint8_t> parsed_alpn_protocols_;
  X509Ptr ca_cert_;
  X509Ptr cert_chain_;
  std::string ca_file_path_;
  std::string cert_chain_file_path_;
};

class ClientContextImpl : public ContextImpl, public ClientContext {
public:
  ClientContextImpl(const std::string& name, Stats::Store& stats, ContextConfig& config);

  SSL* newSsl() const override;

private:
  std::string server_name_indication_;
};

class ServerContextImpl : public ContextImpl, public ServerContext {
public:
  ServerContextImpl(const std::string& name, Stats::Store& stats, ContextConfig& config,
                    Runtime::Loader& runtime);

private:
  int alpnSelectCallback(const unsigned char** out, unsigned char* outlen, const unsigned char* in,
                         unsigned int inlen);

  Runtime::Loader& runtime_;
  std::vector<uint8_t> parsed_alt_alpn_protocols_;
};

} // Ssl
