// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/host.h>
#include <openenclave/trace.h>
#include <stdio.h>

// Include the untrusted log_callback header that is generated
// during the build. This file is generated by calling the
// sdk tool oeedger8r against the log_callback.edl file.
#include "log_callback_u.h"

bool check_simulate_opt(int* argc, const char* argv[])
{
    for (int i = 0; i < *argc; i++)
    {
        if (strcmp(argv[i], "--simulate") == 0)
        {
            fprintf(stdout, "Running in simulation mode\n");
            memmove(&argv[i], &argv[i + 1], (*argc - i) * sizeof(char*));
            (*argc)--;
            return true;
        }
    }
    return false;
}

// This is the function that the enclave will call back into to
// print a message.
void host_hello()
{
    fprintf(stdout, "Enclave called into host to print: Hello!\n");
}

void host_customized_log(
    void* context,
    bool is_enclave,
    const struct tm* t,
    long int usecs,
    oe_log_level_t level,
    uint64_t host_thread_id,
    const char* message)
{
    char time[25];
    strftime(time, sizeof(time), "%Y-%m-%dT%H:%M:%S%z", t);

    FILE* log_file = NULL;
    if (level >= OE_LOG_LEVEL_WARNING)
    {
        log_file = (FILE*)context;
    }
    else
    {
        log_file = stderr;
    }

    fprintf(
        log_file,
        "%s.%06ld, %s, %s, %llx, %s",
        time,
        usecs,
        (is_enclave ? "E" : "H"),
        oe_log_level_strings[level],
        host_thread_id,
        message);
}

static FILE* enc_logfile = NULL;

void host_transfer_logs_to_file(const char* modified_log, size_t size)
{
    fprintf(enc_logfile, "%.*s", (int)size, modified_log);
}

int main(int argc, const char* argv[])
{
    FILE* out_file = fopen("./oe_host_out.txt", "w");
    oe_log_set_callback((void*)out_file, host_customized_log);

    enc_logfile = fopen("./oe_enclave_out.txt", "w");

    oe_result_t result;
    int ret = 1;
    oe_enclave_t* enclave = NULL;

    uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;
    if (check_simulate_opt(&argc, argv))
    {
        flags |= OE_ENCLAVE_FLAG_SIMULATE;
    }

    if (argc != 2)
    {
        fprintf(
            stderr, "Usage: %s enclave_image_path [ --simulate  ]\n", argv[0]);
        goto exit;
    }

    // Create the enclave
    result = oe_create_log_callback_enclave(
        argv[1], OE_ENCLAVE_TYPE_AUTO, flags, NULL, 0, &enclave);
    if (result != OE_OK)
    {
        fprintf(
            stderr,
            "oe_create_log_callback_enclave(): result=%u (%s)\n",
            result,
            oe_result_str(result));
        goto exit;
    }

    // Set callback for enclave logs
    result = enclave_set_log_callback(enclave);

    // Call into the enclave

    result = enclave_hello(enclave);
    if (result != OE_OK)
    {
        fprintf(
            stderr,
            "calling into enclave_hello failed: result=%u (%s)\n",
            result,
            oe_result_str(result));
        goto exit;
    }
    else
    {
        fprintf(
            stdout,
            "Please check ./oe_host_out.txt and ./oe_enclave_out.txt for the "
            "redirected host and enclave logs, respectively.\n");
    }

    ret = 0;

exit:
    // Clean up the enclave if we created one
    if (enclave)
        oe_terminate_enclave(enclave);

    if (out_file)
        fclose(out_file);
    if (enc_logfile)
        fclose(enc_logfile);

    return ret;
}
