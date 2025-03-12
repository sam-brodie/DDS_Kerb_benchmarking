// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#ifdef OPENSSL_API_COMPAT
#undef OPENSSL_API_COMPAT
#endif // ifdef OPENSSL_API_COMPAT
#define OPENSSL_API_COMPAT 10101

//#include <security/authentication/PKIDH.h>

#include <openssl/opensslv.h>



#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define IS_OPENSSL_1_1 1
#define OPENSSL_CONST const
#else
#define IS_OPENSSL_1_1 0
#define OPENSSL_CONST
#endif // if OPENSSL_VERSION_NUMBER >= 0x10100000L

#if OPENSSL_VERSION_NUMBER >= 0x10101040L
#define IS_OPENSSL_1_1_1d 1
#else
#define IS_OPENSSL_1_1_1d 0
#endif // if OPENSSL_VERSION_NUMBER >= 0x10101040L

#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/evp.h>
//
//#include <security/artifact_providers/FileProvider.hpp>
//#include <security/artifact_providers/Pkcs11Provider.hpp>

#include <cassert>
#include <algorithm>
#include <utility>

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION " (" __FILE__ ":" S2(__LINE__) ")"
#define _SecurityException_(str) SecurityException(std::string(str) + LOCATION)

#include <iostream>
#include <chrono>

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <openssl/rand.h>
#include <vector>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>

static inline EVP_PKEY* generate_dh_key(
    int type)
{
    EVP_PKEY_CTX* pctx = nullptr;
    EVP_PKEY* params = nullptr;

    if (type == EVP_PKEY_EC)
    {
        pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
        if (pctx != nullptr)
        {
            if ((1 != EVP_PKEY_paramgen_init(pctx)) ||
                (1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1)) ||
                (1 != EVP_PKEY_paramgen(pctx, &params)))
            {
                //exception = _SecurityException_("Cannot set default parameters: ");
                std::cout << "Cannot set default parameters: " << std::endl;
                EVP_PKEY_CTX_free(pctx);
                return nullptr;
            }
        }
        else
        {
            //exception = _SecurityException_("Cannot allocate EVP parameters");
            std::cout << "Cannot allocate EVP parameters: " << std::endl;
            return nullptr;
        }
    }
    else if (type == EVP_PKEY_DH)
    {
        params = EVP_PKEY_new();
        if (params != nullptr)
        {
            DH* dh = DH_get_2048_256();
            if (dh != nullptr)
            {
#if IS_OPENSSL_1_1_1d
                int dh_type = DH_get0_q(dh) == NULL ? EVP_PKEY_DH : EVP_PKEY_DHX;
                if (EVP_PKEY_assign(params, dh_type, dh) <= 0)
#else
                if (EVP_PKEY_assign_DH(params, dh) <= 0)
#endif // if IS_OPENSSL_1_1_1d
                {
                    //exception = _SecurityException_("Cannot set default parameters: ");
                    std::cout << "Cannot set default parameters: " << std::endl;
                    DH_free(dh);
                    EVP_PKEY_free(params);
                    return nullptr;
                }
            }
        }
        else
        {
            //exception = _SecurityException_("Cannot allocate EVP parameters");
            std::cout << "_SecurityException_: " << std::endl;
            return nullptr;
        }
    }
    else
    {
        //exception = _SecurityException_("Wrong DH kind");
        std::cout << "_SecurityException_: " << std::endl;
        return nullptr;
    }

    EVP_PKEY* keys = nullptr;
    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new(params, NULL);

    if (kctx != nullptr)
    {
        if (1 == EVP_PKEY_keygen_init(kctx))
        {
            if (1 != EVP_PKEY_keygen(kctx, &keys))
            {
                //exception = _SecurityException_("Cannot generate EVP key");
                std::cout << "_SecurityException_: " << std::endl;
            }
        }
        else
        {
            //exception = _SecurityException_("Cannot init EVP key");
            std::cout << "_SecurityException_: " << std::endl;
        }

        EVP_PKEY_CTX_free(kctx);
    }
    else
    {
        //exception = _SecurityException_("Cannot create EVP context");
        std::cout << "_SecurityException_: " << std::endl;
    }

    ERR_clear_error();
    EVP_PKEY_free(params);
    if (pctx != nullptr)
    {
        EVP_PKEY_CTX_free(pctx);
    }
    return keys;
}



// Function to load an X.509 certificate from a file
X509* load_certificate(const std::string& cert_path) {
    FILE* file;
    errno_t err = fopen_s(&file, cert_path.c_str(), "r");
    if (err != 0) {
        std::cerr << "Error: Cannot open certificate file: " << cert_path << "\n";
        return nullptr;
    }

    X509* cert = PEM_read_X509(file, nullptr, nullptr, nullptr);
    fclose(file);

    if (!cert) {
        std::cerr << "Error: Failed to parse X.509 certificate\n";
    }

    return cert;
}

// Function to load a CA certificate into a store
X509_STORE* load_ca_cert(const std::string& ca_cert_path) {
    X509_STORE* store = X509_STORE_new();
    if (!store) {
        std::cerr << "Error: Failed to create X509 store\n";
        return nullptr;
    }

    X509* ca_cert = load_certificate(ca_cert_path);
    if (!ca_cert) {
        X509_STORE_free(store);
        return nullptr;
    }

    if (X509_STORE_add_cert(store, ca_cert) != 1) {
        std::cerr << "Error: Failed to add CA certificate to store\n";
        X509_free(ca_cert);
        X509_STORE_free(store);
        return nullptr;
    }

    X509_free(ca_cert);  // Store holds a reference now
    return store;
}


int main()
{
    std::ofstream my_dh_ResultsFile;
    my_dh_ResultsFile.open("01_dh_results_millisec.csv");

    std::chrono::duration<double, std::milli> duration;

    /////////////////////////////////
    ///// DH keypair generation /////
    /////////////////////////////////
    int dh_loops = 100;
    std::cout << ".....generating DH key pair " << dh_loops << " times..." << std::endl;
    //my_dh_ResultsFile << "DH key pair Execution time (ms): " << "\n";
    // Start timing
    for (int i = 0; i < dh_loops; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        //for(int i = 1; i<100; i++)
            //EVP_PKEY* x = generate_dh_key(EVP_PKEY_EC);
        EVP_PKEY* x = generate_dh_key(EVP_PKEY_DH);
        // End timing
        //endTimer(start, "\tDH key pair Execution time: ");
        auto end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        //std::cout << "\tDH key pair Execution time2: " << duration.count() << " ms" << std::endl;

        my_dh_ResultsFile << duration.count() << ", ";
    }

    std::cout << "..... DH key pair finished...\nLatest time :" << duration.count() << " ms" << std::endl;
    my_dh_ResultsFile.close();

    /////////////////////////////////
    ///// x509 Cert verification/////
    /////////////////////////////////
    std::cout << ".....verifying certiciates..." << std::endl;
    int identityCert_loops = 100;
    std::ofstream my_cert_ResultsFile;
    my_cert_ResultsFile.open("02_cert_results_millisec.csv");
    std::string cert_path = "hundred_keystores/demo_keystore_100/enclaves/talker_listener/talker/cert.pem";  // Path to client certificate
    std::string ca_cert_path = "hundred_keystores/demo_keystore_100/public/ca.cert.pem";   // Path to CA certificate

    for (int i = 0; i < identityCert_loops; i++)
    {
        cert_path = "hundred_keystores/demo_keystore_" + std::to_string(i + 1) + "/enclaves/talker_listener/talker/cert.pem";
        ca_cert_path = "hundred_keystores/demo_keystore_" + std::to_string(i + 1) + "/public/ca.cert.pem";

        X509* cert = load_certificate(cert_path);
        if (!cert) return false;
        X509_STORE* store = load_ca_cert(ca_cert_path);
        X509_STORE_CTX* ctx = X509_STORE_CTX_new();
        if (X509_STORE_CTX_init(ctx, store, cert, nullptr) != 1)
            return 0;

        auto start = std::chrono::high_resolution_clock::now();

        int result = X509_verify_cert(ctx);

        auto end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        my_cert_ResultsFile << duration.count() << ", ";

        X509_STORE_CTX_free(ctx);
        X509_STORE_free(store);
        X509_free(cert);

        if (result == 1) {
            //std::cout << "Success: Certificate is valid and signed by the CA!" << std::endl;
        }
        else {
            std::cerr << "Error: Certificate verification failed: "
                << X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)) << "\n";
            return false;
        }
    }

    std::cout << "..... verifying certiciates finished..." << std::endl;
    my_cert_ResultsFile.close();




    /////////////////////////////////
    ///// AES 256 encryption    /////
    /////////////////////////////////
    std::cout << ".....AES 256 encryption..." << std::endl;
    int aes_loops = 100;
    std::ofstream my_aes_enc_ResultsFile, my_aes_dec_ResultsFile;
    my_aes_enc_ResultsFile.open("04_aes_encrypt_results_millisec.csv");
    my_aes_dec_ResultsFile.open("05_aes_decrypt_results_millisec.csv");
    // Generate a random AES key (256 bits) and IV (128 bits)
    unsigned char key[32]; // 32 bytes for AES-256
    unsigned char iv[16];  // 16 bytes IV
    // AES block size
    constexpr int AES_BLOCK_SIZE = 16;
    std::string inputText = "<dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://www.omg.org/spec/DDS-SECURITY/20170901/omg_shared_ca_permissions.xsd\"><permissions><grant name = \"/talker_listener/talker\"><subject_name>CN = / talker_listener / talker< / subject_name><validity>< not_before>2025 - 03 - 05T12 : 15 : 44 < / not_before >< not_after>2035 - 03 - 04T12 : 15 : 44 < / not_after >< / validity><allow_rule><domains>< id>0 < / id >< / domains><publish><topics><topic>rq/*/_action/cancel_goalRequest</topic><topic>rq/*/_action / get_resultRequest< / topic><topic>rq/*/_action/send_goalRequest</topic><topic>rq/*Request</topic><topic>rr/*/_action / cancel_goalReply< / topic><topic>rr/*/_action/get_resultReply</topic><topic>rr/*/_action / send_goalReply< / topic><topic>rt/*/_action/feedback</topic><topic>rt/*/_action / status< / topic><topic>rr/*Reply</topic><topic>rt/*</topic></topics></publish><subscribe><topics><topic>rq/*/_action / cancel_goalRequest< / topic><topic>rq/*/_action/get_resultRequest</topic><topic>rq/*/_action / send_goalRequest< / topic><topic>rq/*Request</topic><topic>rr/*/_action / cancel_goalReply< / topic><topic>rr/*/_action/get_resultReply</topic><topic>rr/*/_action / send_goalReply< / topic><topic>rt/*/_action/feedback</topic><topic>rt/*/_action / status< / topic><topic>rr/*Reply</topic><topic>rt/*</topic></topics></subscribe></allow_rule><allow_rule><domains><id>0</id></domains><publish><topics><topic>ros_discovery_info</topic></topics></publish><subscribe><topics><topic>ros_discovery_info</topic></topics></subscribe></allow_rule><default>DENY</default></grant></permissions></dds>";
    std::vector<unsigned char> plaintext_in(inputText.begin(), inputText.end());
    std::cout << "input text: " << inputText << std::endl;

    for (int i = 0; i < 100; i++)
    {
        if (RAND_bytes(key, sizeof(key)) != 1 || RAND_bytes(iv, sizeof(iv)) != 1) 
            return 1;

        auto start = std::chrono::high_resolution_clock::now();

        EVP_CIPHER_CTX* ctx_enc = EVP_CIPHER_CTX_new();
        if (!ctx_enc)
            return 1;

        // Initialize AES-256-CBC encryption
        if (EVP_EncryptInit_ex(ctx_enc, EVP_aes_256_cbc(), nullptr, key, iv) != 1)
            return 1;
        std::vector<unsigned char> ciphertext(plaintext_in.size() + AES_BLOCK_SIZE);
        int len = 0, ciphertext_len = 0;
        // Encrypt the data
        if (EVP_EncryptUpdate(ctx_enc, ciphertext.data(), &len, plaintext_in.data(), plaintext_in.size()) != 1)
            return 1;
        ciphertext_len += len;
        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx_enc, ciphertext.data() + len, &len) != 1)
            return 1;
        ciphertext_len += len;
        // Resize the ciphertext buffer to actual size
        ciphertext.resize(ciphertext_len);


        auto end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        my_aes_enc_ResultsFile << duration.count() << ", ";

        EVP_CIPHER_CTX_free(ctx_enc);


    /////////////////////////////////
    ///// AES 256 decryption    /////
    /////////////////////////////////


        start = std::chrono::high_resolution_clock::now();

        std::vector<unsigned char> plaintext_out(ciphertext.size());
        len = 0;
        int plaintext_len = 0;

        EVP_CIPHER_CTX* ctx_dec = EVP_CIPHER_CTX_new();
        if (!ctx_dec)
            return 1;

        // Initialize AES-256-CBC decryption
        if (EVP_DecryptInit_ex(ctx_dec, EVP_aes_256_cbc(), nullptr, key, iv) != 1)
            return 1;
        // Decrypt the data
        if (EVP_DecryptUpdate(ctx_dec, plaintext_out.data(), &len, ciphertext.data(), ciphertext.size()) != 1)
            return 1;
        plaintext_len += len;
        // Finalize decryption
        if (EVP_DecryptFinal_ex(ctx_dec, plaintext_out.data() + len, &len) != 1)
            return 1;
        plaintext_len += len;
        // Resize the plaintext buffer to actual size
        plaintext_out.resize(plaintext_len);

        EVP_CIPHER_CTX_free(ctx_dec);

        end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        my_aes_dec_ResultsFile << duration.count() << ", ";
    }

    std::cout << "aes enc complete sample time: " << duration.count() << " ms" << std::endl;
    my_aes_enc_ResultsFile.close();
    my_aes_dec_ResultsFile.close();



    /////////////////////////////////
    ///// PKCS7 permisions verify /////
    /////////////////////////////////

    int permissionsCert_loops = 100;
    std::ofstream permissions_ResultsFile;
    permissions_ResultsFile.open("03_plcs7_permissions_results_millisec.csv");

    for (int i = 0; i < permissionsCert_loops; i++)
    {
        std::string perm_doc_path = "hundred_keystores/demo_keystore_" + std::to_string(i + 1) + "/enclaves/talker_listener/talker/permissions.p7s";
        ca_cert_path = "hundred_keystores/demo_keystore_" + std::to_string(i + 1) + "/public/ca.cert.pem";
        BIO* in = BIO_new_file(perm_doc_path.c_str(), "r");
        X509_STORE* store = load_ca_cert(ca_cert_path);
        X509_STORE_CTX* ctx = X509_STORE_CTX_new();

        if (X509_STORE_CTX_init(ctx, store, nullptr, nullptr) != 1)
            return 1;

        BIO* indata = nullptr;
        PKCS7* p7 = SMIME_read_PKCS7(in, &indata);
        BIO* out = BIO_new(BIO_s_mem());

        auto start = std::chrono::high_resolution_clock::now();

        if (!PKCS7_verify(p7, nullptr, store, indata, out, PKCS7_TEXT | PKCS7_NOVERIFY))
        {
            std::cout << "ERROR VERIFYING PERMISSIONS DOCUMENT " << std::endl;
            return 1;
        }

        auto end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        permissions_ResultsFile << duration.count() << ", ";

        X509_STORE_CTX_free(ctx);
        X509_STORE_free(store);
    }
    permissions_ResultsFile.close();

    std::cout << "COMPLETE " << std::endl;
    return 0;
}