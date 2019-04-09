
#include <iostream>
#include <unistd.h>
#include "statsd_client.h"

int main(void)
{
    std::cout << "running..." << std::endl;

    statsd::StatsdClient client;
    statsd::StatsdClient client2("127.0.0.1", 8125, "myproject.abx.", true);

    client.count("count1", 123, 1.0);
    client.count("count2", 125, 1.0);
    client.gauge("speed", 10);
    int i;
    for (i=0; i<1000; i++)
        client2.timing("request", i);
    sleep(1);
    client.inc("count1", 1.0);
    client2.dec("count2", 1.0);
//    for(i=0; i<1000; i++) {
//        client2.count("count3", i, 0.8);
//    }

    std::cout << "done" << std::endl;
    return 0;
}
