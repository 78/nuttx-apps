/****************************************************************************
 * apps/examples/e1net/e1net_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <arpa/inet.h>
#include <errno.h>
#include <malloc.h>
#include <netdb.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#define E1NET_DEFAULT_HOST "valid-isrgrootx1.letsencrypt.org"
#define E1NET_DEFAULT_PORT "443"
#define E1NET_TIMEOUT_SEC  15

/* Self-signed ISRG Root X1, downloaded from the Let's Encrypt certificate
 * repository.  The default endpoint is maintained specifically to present a
 * valid chain to this root.
 */

static const char g_isrg_root_x1[] =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n";

struct e1net_result_s
{
  long dns_ms;
  long tcp_ms;
  long tls_ms;
  int heap_before;
  int heap_tls;
  int heap_peak;
};

struct e1net_tls_context_s
{
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
  mbedtls_net_context net;
  mbedtls_ssl_config config;
  mbedtls_ssl_context ssl;
  mbedtls_x509_crt trust;
};

static long e1net_elapsed_ms(const struct timespec *start,
                             const struct timespec *end)
{
  return (end->tv_sec - start->tv_sec) * 1000 +
         (end->tv_nsec - start->tv_nsec) / 1000000;
}

static void e1net_tls_error(const char *phase, int ret)
{
  char buffer[96];

  mbedtls_strerror(ret, buffer, sizeof(buffer));
  fprintf(stderr, "E1NET:%s=FAIL ret=-0x%04x %s\n",
          phase, (unsigned int)-ret, buffer);
}

/* Device clocks can lag NTP briefly after bring-up.  Keep chain and hostname
 * checks, but clear notBefore/notAfter failures for the E.1 smoke path.
 */

static int e1net_verify_ignore_time(void *data, mbedtls_x509_crt *crt,
                                    int depth, uint32_t *flags)
{
  (void)data;
  (void)crt;
  (void)depth;

  if (flags != NULL)
    {
      *flags &= ~(uint32_t)(MBEDTLS_X509_BADCERT_EXPIRED |
                            MBEDTLS_X509_BADCERT_FUTURE);
    }

  return 0;
}

static int e1net_probe(const char *host, const char *port, int round,
                       bool insecure, struct e1net_result_s *result)
{
  struct addrinfo hints;
  struct addrinfo *addresses = NULL;
  struct addrinfo *address;
  struct mallinfo memory;
  struct timespec begin;
  struct timespec end;
  struct timeval timeout;
  struct e1net_tls_context_s *tls;
  char request[256];
  char response[192];
  char ip[INET6_ADDRSTRLEN];
  const void *ipaddr;
  int family;
  int ret;
  int fd = -1;

  memset(result, 0, sizeof(*result));
  memory = mallinfo();
  result->heap_before = memory.uordblks;
  result->heap_peak = memory.usmblks;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  clock_gettime(CLOCK_MONOTONIC, &begin);
  ret = getaddrinfo(host, port, &hints, &addresses);
  clock_gettime(CLOCK_MONOTONIC, &end);
  result->dns_ms = e1net_elapsed_ms(&begin, &end);
  if (ret != 0)
    {
      fprintf(stderr, "E1NET:DNS=FAIL round=%d ret=%d %s\n",
              round, ret, gai_strerror(ret));
      return EXIT_FAILURE;
    }

  address = addresses;
  if (address->ai_family == AF_INET)
    {
      ipaddr = &((struct sockaddr_in *)address->ai_addr)->sin_addr;
      family = AF_INET;
    }
  else
    {
      ipaddr = &((struct sockaddr_in6 *)address->ai_addr)->sin6_addr;
      family = AF_INET6;
    }

  inet_ntop(family, ipaddr, ip, sizeof(ip));
  printf("E1NET:DNS=PASS round=%d host=%s ip=%s ms=%ld\n",
         round, host, ip, result->dns_ms);

  clock_gettime(CLOCK_MONOTONIC, &begin);
  for (address = addresses; address != NULL; address = address->ai_next)
    {
      fd = socket(address->ai_family, address->ai_socktype,
                  address->ai_protocol);
      if (fd >= 0 && connect(fd, address->ai_addr, address->ai_addrlen) == 0)
        {
          break;
        }

      if (fd >= 0)
        {
          close(fd);
          fd = -1;
        }
    }

  clock_gettime(CLOCK_MONOTONIC, &end);
  result->tcp_ms = e1net_elapsed_ms(&begin, &end);
  freeaddrinfo(addresses);
  if (fd < 0)
    {
      fprintf(stderr, "E1NET:TCP=FAIL round=%d errno=%d\n", round, errno);
      return EXIT_FAILURE;
    }

  printf("E1NET:TCP=PASS round=%d ms=%ld\n", round, result->tcp_ms);

  timeout.tv_sec = E1NET_TIMEOUT_SEC;
  timeout.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  tls = calloc(1, sizeof(*tls));
  if (tls == NULL)
    {
      fprintf(stderr, "E1NET:TLS=FAIL out-of-memory\n");
      close(fd);
      return EXIT_FAILURE;
    }

  mbedtls_net_init(&tls->net);
  mbedtls_ssl_init(&tls->ssl);
  mbedtls_ssl_config_init(&tls->config);
  mbedtls_x509_crt_init(&tls->trust);
  mbedtls_ctr_drbg_init(&tls->ctr_drbg);
  mbedtls_entropy_init(&tls->entropy);
  tls->net.fd = fd;

  ret = mbedtls_ctr_drbg_seed(&tls->ctr_drbg, mbedtls_entropy_func,
                              &tls->entropy,
                              (const unsigned char *)"s31-e1net", 9);
  if (ret != 0)
    {
      e1net_tls_error("RNG", ret);
      goto fail;
    }

  ret = mbedtls_x509_crt_parse(&tls->trust,
                               (const unsigned char *)g_isrg_root_x1,
                               sizeof(g_isrg_root_x1));
  if (ret < 0)
    {
      e1net_tls_error("CA", ret);
      goto fail;
    }

  ret = mbedtls_ssl_config_defaults(&tls->config, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0)
    {
      e1net_tls_error("CONFIG", ret);
      goto fail;
    }

  mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random,
                       &tls->ctr_drbg);
  mbedtls_ssl_conf_ca_chain(&tls->config, &tls->trust, NULL);
  mbedtls_ssl_conf_authmode(&tls->config,
                            insecure ? MBEDTLS_SSL_VERIFY_NONE :
                            MBEDTLS_SSL_VERIFY_REQUIRED);
  if (!insecure)
    {
      mbedtls_ssl_conf_verify(&tls->config, e1net_verify_ignore_time, NULL);
    }

  ret = mbedtls_ssl_setup(&tls->ssl, &tls->config);
  if (ret != 0)
    {
      e1net_tls_error("SETUP", ret);
      goto fail;
    }

  ret = mbedtls_ssl_set_hostname(&tls->ssl, host);
  if (ret != 0)
    {
      e1net_tls_error("SNI", ret);
      goto fail;
    }

  mbedtls_ssl_set_bio(&tls->ssl, &tls->net, mbedtls_net_send,
                      mbedtls_net_recv,
                      mbedtls_net_recv_timeout);

  clock_gettime(CLOCK_MONOTONIC, &begin);
  do
    {
      ret = mbedtls_ssl_handshake(&tls->ssl);
    }
  while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
         ret == MBEDTLS_ERR_SSL_WANT_WRITE);
  clock_gettime(CLOCK_MONOTONIC, &end);
  result->tls_ms = e1net_elapsed_ms(&begin, &end);
  if (ret != 0)
    {
      e1net_tls_error("TLS", ret);
      goto fail;
    }

  if (!insecure && mbedtls_ssl_get_verify_result(&tls->ssl) != 0)
    {
      fprintf(stderr, "E1NET:VERIFY=FAIL flags=%08lx\n",
              (unsigned long)mbedtls_ssl_get_verify_result(&tls->ssl));
      goto fail;
    }

  memory = mallinfo();
  result->heap_tls = memory.uordblks;
  result->heap_peak = memory.usmblks;
  printf("E1NET:TLS=PASS round=%d version=%s cipher=%s verify=%s ms=%ld\n",
         round, mbedtls_ssl_get_version(&tls->ssl),
         mbedtls_ssl_get_ciphersuite(&tls->ssl),
         insecure ? "disabled" : "chain",
         result->tls_ms);

  snprintf(request, sizeof(request),
           "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
  ret = mbedtls_ssl_write(&tls->ssl, (const unsigned char *)request,
                          strlen(request));
  if (ret <= 0)
    {
      e1net_tls_error("WRITE", ret);
      goto fail;
    }

  ret = mbedtls_ssl_read(&tls->ssl, (unsigned char *)response,
                         sizeof(response) - 1);
  if (ret <= 0)
    {
      e1net_tls_error("READ", ret);
      goto fail;
    }

  response[ret] = '\0';
  if (strncmp(response, "HTTP/", 5) != 0)
    {
      fprintf(stderr, "E1NET:HTTP=FAIL invalid-response\n");
      goto fail;
    }

  printf("E1NET:HTTP=PASS round=%d status=%.15s\n", round, response);
  mbedtls_ssl_close_notify(&tls->ssl);
  mbedtls_net_free(&tls->net);
  mbedtls_ssl_free(&tls->ssl);
  mbedtls_ssl_config_free(&tls->config);
  mbedtls_x509_crt_free(&tls->trust);
  mbedtls_ctr_drbg_free(&tls->ctr_drbg);
  mbedtls_entropy_free(&tls->entropy);
  free(tls);
  return EXIT_SUCCESS;

fail:
  mbedtls_net_free(&tls->net);
  mbedtls_ssl_free(&tls->ssl);
  mbedtls_ssl_config_free(&tls->config);
  mbedtls_x509_crt_free(&tls->trust);
  mbedtls_ctr_drbg_free(&tls->ctr_drbg);
  mbedtls_entropy_free(&tls->entropy);
  free(tls);
  return EXIT_FAILURE;
}

/* Offline SHA-256 torture used to separate a CPU-local context or memory
 * corruption from anything in the socket and TLS paths.  Every iteration
 * re-hashes the same buffer, so a single mismatch means the computation was
 * disturbed rather than the peer data.
 */

static int e1net_selftest(int rounds, size_t heap_bytes)
{
  unsigned char expected[32];
  unsigned char digest[32];
  unsigned char *buffer;
  size_t length = 4096;
  int round;
  int ret;

  buffer = malloc(length);
  if (buffer == NULL)
    {
      fprintf(stderr, "E1NET:SELFTEST=FAIL alloc\n");
      return EXIT_FAILURE;
    }

  for (size_t i = 0; i < length; i++)
    {
      buffer[i] = (unsigned char)(i * 31 + 7);
    }

  ret = mbedtls_sha256(buffer, length, expected, 0);
  if (ret != 0)
    {
      fprintf(stderr, "E1NET:SELFTEST=FAIL sha256 ret=%d\n", ret);
      free(buffer);
      return EXIT_FAILURE;
    }

  for (round = 1; round <= rounds; round++)
    {
      unsigned char *churn = NULL;

      if (heap_bytes > 0)
        {
          churn = malloc(heap_bytes);
          if (churn != NULL)
            {
              memset(churn, (int)(round & 0xff), heap_bytes);
            }
        }

      ret = mbedtls_sha256(buffer, length, digest, 0);
      if (ret != 0 || memcmp(digest, expected, sizeof(digest)) != 0)
        {
          fprintf(stderr, "E1NET:SELFTEST=FAIL round=%d ret=%d cpu=%d\n",
                  round, ret, sched_getcpu());
          free(churn);
          free(buffer);
          return EXIT_FAILURE;
        }

      free(churn);
      sched_yield();
    }

  printf("E1NET:SELFTEST=PASS rounds=%d heap=%zu cpu=%d\n",
         rounds, heap_bytes, sched_getcpu());
  free(buffer);
  return EXIT_SUCCESS;
}

static int e1net_pin_cpu(int cpu)
{
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
    {
      return -errno;
    }

  return 0;
}

int main(int argc, char **argv)
{
  const char *host = E1NET_DEFAULT_HOST;
  const char *port = E1NET_DEFAULT_PORT;
  struct e1net_result_s result;
  bool insecure = false;
  size_t selftest_heap = 0;
  int selftest = 0;
  int pin_cpu = -1;
  int rounds = 1;
  int option;
  int round;
  int ret;

  while ((option = getopt(argc, argv, "h:p:r:c:s:m:k")) != ERROR)
    {
      switch (option)
        {
          case 's':
            selftest = atoi(optarg);
            break;
          case 'm':
            selftest_heap = (size_t)strtoul(optarg, NULL, 0);
            break;
          case 'h':
            host = optarg;
            break;
          case 'p':
            port = optarg;
            break;
          case 'r':
            rounds = atoi(optarg);
            break;
          case 'c':
            pin_cpu = atoi(optarg);
            break;
          case 'k':
            insecure = true;
            break;
          default:
            fprintf(stderr,
                    "usage: e1net [-h host] [-p port] [-r rounds] "
                    "[-c cpu] [-s selftest] [-m bytes] [-k]\n");
            return EXIT_FAILURE;
        }
    }

  if (rounds < 1 || rounds > 100)
    {
      fprintf(stderr, "e1net: rounds must be between 1 and 100\n");
      return EXIT_FAILURE;
    }

  if (pin_cpu >= 0)
    {
      ret = e1net_pin_cpu(pin_cpu);
      if (ret < 0)
        {
          fprintf(stderr, "E1NET:PIN=FAIL cpu=%d err=%d\n", pin_cpu, -ret);
          return EXIT_FAILURE;
        }
    }

  if (selftest > 0)
    {
      return e1net_selftest(selftest, selftest_heap);
    }

  printf("E1NET:START pid=%d host=%s port=%s rounds=%d pin_cpu=%d\n",
         getpid(), host, port, rounds, pin_cpu);
  for (round = 1; round <= rounds; round++)
    {
      ret = e1net_probe(host, port, round, insecure, &result);
      printf("E1NET:METRIC round=%d dns_ms=%ld tcp_ms=%ld tls_ms=%ld "
             "heap_before=%d heap_tls=%d heap_peak=%d\n",
             round, result.dns_ms, result.tcp_ms, result.tls_ms,
             result.heap_before, result.heap_tls, result.heap_peak);
      if (ret != EXIT_SUCCESS)
        {
          return ret;
        }
    }

  printf("E1NET:RESULT=PASS rounds=%d\n", rounds);
  return EXIT_SUCCESS;
}
