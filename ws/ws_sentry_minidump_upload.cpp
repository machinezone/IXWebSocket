/*
 *  ws_sentry_minidump_upload.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <fstream>
#include <ixsentry/IXSentryClient.h>
#include <jsoncpp/json/json.h>
#include <spdlog/spdlog.h>
#include <sstream>


namespace
{
    // Assume the file exists
    std::string readBytes(const std::string& path)
    {
        std::vector<uint8_t> memblock;
        std::ifstream file(path);

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        memblock.resize(size);

        file.read((char*) &memblock.front(), static_cast<std::streamsize>(size));

        std::string bytes(memblock.begin(), memblock.end());
        return bytes;
    }
} // namespace

namespace ix
{
    int ws_sentry_minidump_upload(const std::string& metadataPath,
                                  const std::string& minidump,
                                  const std::string& project,
                                  const std::string& key,
                                  bool verbose)
    {
        SentryClient sentryClient((std::string()));

        // Read minidump file from disk
        std::string minidumpBytes = readBytes(minidump);

        // Read json data
        std::string sentryMetadata = readBytes(metadataPath);

        std::atomic<bool> done(false);

        sentryClient.uploadMinidump(
            sentryMetadata,
            minidumpBytes,
            project,
            key,
            verbose,
            [verbose, &done](const HttpResponsePtr& response) {
                if (verbose)
                {
                    for (auto it : response->headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }

                    spdlog::info("Upload size: {}", response->uploadSize);
                    spdlog::info("Download size: {}", response->downloadSize);

                    spdlog::info("Status: {}", response->statusCode);
                    if (response->errorCode != HttpErrorCode::Ok)
                    {
                        spdlog::info("error message: {}", response->errorMsg);
                    }

                    if (response->headers["Content-Type"] != "application/octet-stream")
                    {
                        spdlog::info("payload: {}", response->payload);
                    }
                }

                if (response->statusCode != 200)
                {
                    spdlog::error("Error sending data to sentry: {}", response->statusCode);
                    spdlog::error("Status: {}", response->statusCode);
                    spdlog::error("Response: {}", response->payload);
                }
                else
                {
                    spdlog::info("Event sent to sentry");
                }

                done = true;
            });

        int i = 0;

        while (!done)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);

            if (i++ > 5000) break; // wait 5 seconds max
        }

        if (!done)
        {
            spdlog::error("Error: timing out trying to sent a crash to sentry");
        }

        return 0;
    }
} // namespace ix
