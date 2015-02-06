#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PERIOD 1
#define BUFFER 512000	// /proc/cpuinfo and diskstats should fit in here

int
main(int argc, char **argv)
{
	FILE	*f;
	int	i, n, ncpu;
	char	*buf, *ptr, load[17];	// 16 characters per each output field
unsigned long	kbr1, kbr2, kbw1, kbw2, trs1, trs2, tws1, tws2, cts1, cts2, hz;
struct	timespec	tick, tock;

	if (argc < 2) {
		fprintf(stderr, "\nUsage: %s <block_device_name_from_/proc/diskstats>\n\n", argv[0]);
		return 1;
	}
	/* getting number of cores */
	if ( (buf = malloc(BUFFER)) == NULL) err(1, "failed to malloc %d bytes", BUFFER);
	if ( (f = fopen("/proc/cpuinfo", "r")) == NULL) err(1, "fopen cpuinfo");
	if ( (i = fread(buf, 1, BUFFER, f)) == 0) errx(1, "fread cpuinfo");
	if (i == BUFFER) errx(1, "BUFFER overflow reading cpuinfo");
	fclose(f);
	buf[i] = 0;	// making a string
	for (ncpu = 0, ptr = buf; (ptr = strstr(ptr, "rocessor")) != NULL; ptr++, ncpu++);
	printf("\nCPU crs: %d\nJiffies: %lu\n\n", ncpu, (hz = sysconf(_SC_CLK_TCK)));
	/* main loop */
	printf("        %%,  Load          kBread       kBwritten");
	printf("    %%timeReading    %%timeWriting      %%CPUtimeIO\n");
	if (clock_gettime(CLOCK_MONOTONIC, &tick) < 0) err(1, "clock_tick");
	kbr1 = kbw1 = trs1 = tws1 = cts1 = 0;
	for (; ;) {
		if (clock_gettime(CLOCK_MONOTONIC, &tock) < 0) err(1, "clock_tock");
		/* tick = tock - tick */
		tick.tv_sec = tock.tv_sec - tick.tv_sec;
		if ( (tick.tv_nsec = tock.tv_nsec - tick.tv_nsec) < 0) {
			tick.tv_sec--;
			tick.tv_nsec += 1000000000;
		}
		tick.tv_sec = tick.tv_nsec / 1000000 + tick.tv_sec * 1000;	// milliseconds

		n = (tick.tv_sec / PERIOD + 499) / 1000;	// lines at this iteration (see quasar)
		for (i = 0; i < n; i++)
			printf("%*s%*lu%*lu%*lu%*lu%*lu\n", 16, load, 16, kbr2 / n / 2,\
				16, kbw2 / n / 2, 16, trs2 * 100 / tick.tv_sec, 16,\
				tws2 * 100 / tick.tv_sec, 16, cts2 * 100000 / ncpu / hz / tick.tv_sec);
		fflush(stdout);
		/* taking measurements */
		if ( (n = open("/proc/loadavg", O_RDONLY)) < 0) err(1, "open loadavg");
		if ( (i = read(n, buf, BUFFER)) < 0) err(1, "read loadavg");
		close(n);
		buf[i] = '\0';
		if (sscanf(buf, "%*s %*s %*s %d/", &n) != 1 || sscanf(buf, "%*s %*s %*s %*d/%d", &i) != 1)
			errx(1, "sscanf loadavg");
		snprintf(load, sizeof(load), "%d /%d", (n - 1) * 100 / ncpu, i);

		if ( (n = open("/proc/stat", O_RDONLY)) < 0) err(1, "open stat");
		if ( (i = read(n, buf, BUFFER)) < 0) err(1, "read stat");
		close(n);
		buf[i] = '\0';
		if (sscanf(buf, "%*s %*s %*s %*s %*s %lu", &cts2) != 1) errx(1, "sscanf stat");
		cts2 -= cts1;	// cts2 gets the diff between NOW & cts1
		cts1 += cts2;	// cts1 becomes NOW

		if ( (n = open("/proc/diskstats", O_RDONLY)) < 0) err(1, "open diskstats");
		if ( (i = read(n, buf, BUFFER)) < 0) err(1, "read diskstats");
		close(n);
		buf[i] = '\0';
		if ( (ptr = strstr(buf, argv[1])) == NULL) errx(1, "no %s in /proc/diskstats", argv[1]);
		if (sscanf(ptr, "%*s %*s %*s %lu %lu %*s %*s %lu %lu", &kbr2, &trs2, &kbw2, &tws2) != 4)
			errx(1, "sscanf diskstats");
		kbr2 -= kbr1; kbr1 += kbr2;
		trs2 -= trs1; trs1 += trs2;
		kbw2 -= kbw1; kbw1 += kbw2;
		tws2 -= tws1; tws1 += tws2;
		/* sleeping up to PERIOD */
		memcpy(&tick, &tock, sizeof(struct timespec));
		tock.tv_sec += PERIOD;
		while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tock, NULL) < 0 && errno == EINTR);
	}
	return 0;
}
