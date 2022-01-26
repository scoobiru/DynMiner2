#include "cStatDisplay.h"
#include "cSubmitter.h"

string cStatDisplay::seconds_to_uptime(int n) {
    int days = n / (24 * 3600);

    n = n % (24 * 3600);
    int hours = n / 3600;

    n %= 3600;
    int minutes = n / 60;

    n %= 60;
    int seconds = n;

    string uptimeString;
    if (days > 0) {
        uptimeString = std::to_string(days) + "d" + std::to_string(hours) + "h" + std::to_string(minutes) + "m"
            + std::to_string(seconds) + "s";
    }
    else if (hours > 0) {
        uptimeString = std::to_string(hours) + "h" + std::to_string(minutes) + "m" + std::to_string(seconds) + "s";
    }
    else if (minutes > 0) {
        uptimeString = std::to_string(minutes) + "m" + std::to_string(seconds) + "s";
    }
    else {
        uptimeString = std::to_string(seconds) + "s";
    }

    return (uptimeString);
}

void cStatDisplay::displayStats(cSubmitter *submitter, string mode) {

    time_t now;
    time_t start;

    time(&start);

#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

	while (true)
	{
        std::this_thread::sleep_for(std::chrono::seconds(1));

        uint64_t nonce = nonce_count;

        struct tm* timeinfo;
        char timestamp[80];
        time(&now);
        timeinfo = localtime(&now);
        strftime(timestamp, 80, "%F %T", timeinfo);
        char display[256];
        double hashrate = (double)nonce / (double)(now - start);
        if (hashrate >= tb)
            sprintf(display, "%.2f TH/s", (double)hashrate / tb);
        else if (hashrate >= gb && hashrate < tb)
            sprintf(display, "%.2f GH/s", (double)hashrate / gb);
        else if (hashrate >= mb && hashrate < gb)
            sprintf(display, "%.2f MH/s", (double)hashrate / mb);
        else if (hashrate >= kb && hashrate < mb)
            sprintf(display, "%.2f KH/s", (double)hashrate / kb);
        else if (hashrate < kb)
            sprintf(display, "%.2f H/s ", hashrate);
        else
            sprintf(display, "%.2f H/s", hashrate);

        std::string uptime = seconds_to_uptime(difftime(now, start));

        SET_COLOR(LIGHTBLUE);
        printf("%s: ", timestamp);
        SET_COLOR(GREEN);
        printf("%s", display);
        // SET_COLOR(LIGHTGRAY);
        // printf(" | ");
        // SET_COLOR(LIGHTGREEN);
        // printf("%d", dynProgram->height);
        SET_COLOR(LIGHTGRAY);
        printf(" | ");
        SET_COLOR(BLUE);
        printf("Uptime: %6s", uptime.c_str());
        SET_COLOR(LIGHTGRAY);
        printf(" | ");
        SET_COLOR(LIGHTGREEN);
        printf("S: %4lu", share_count.load(std::memory_order_relaxed));
        SET_COLOR(GREEN);
        SET_COLOR(LIGHTGRAY);
        printf(" | ");
        printf("A: %-4d", accepted_share_count.load(std::memory_order_relaxed));
        SET_COLOR(LIGHTGRAY);
        printf(" | ");
        SET_COLOR(RED);
        printf("R: %4d", rejected_share_count.load(std::memory_order_relaxed));
        SET_COLOR(LIGHTGRAY);
        printf(" | ");
        SET_COLOR(LIGHTGREEN);
        printf("N:%-12lu\n", nonce);
        SET_COLOR(YELLOW);

	}

}