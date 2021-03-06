BINS := can-bounce-test can-monitor can2eth can2udp can4lnx2udp can2serial clone-cs2-config clone-cs2-trigger \
        clone-ms2-loco read-cs2-ini gb2-update ir-tester cs2ping bpi-rs485 scan2lan can2lan \
        ms2wifi virtual_canbuster virtual_gfp virtual_gfp3 virtual_links88 virtual_ms2 \
        get-cs-data get-cs-config pio-i2c-udp read_lococard s88udp-bpi s88udp-rpi s88test-bpi \
        wake-up-links88 virtual_s88udp gpio-poll lan-schnitte xntcpd s88can canpanel

PREFIX=/usr/sbin

BIN = $@

all:	$(BINS)

install: all
	mkdir -p $(DESTDIR)/$(PREFIX)
	cp $(BINS) $(DESTDIR)/$(PREFIX)

# CFLAGS+=-Wall -Wextra -Wmaybe-uninitialized -pedantic-errors -std=gnu99 -g
CFLAGS+=-g -O2 -fstack-protector-strong -Wformat -Werror=format-security -Wall -Wextra -Wmaybe-uninitialized -std=gnu99 -pedantic-errors -Wdate-time

size_of-test: size_of-test.o
	$(CC) $(CFLAGS) -o size_of-test size_of-test.o -lpcap

test_scanf: test_scanf.o
	$(CC) $(CFLAGS) -o test_scanf test_scanf.o

read_cs2_dump: read_cs2_dump.o
	$(CC) $(CFLAGS) -o read_cs2_dump read_cs2_dump.o -lpcap

bpi-rs485: bpi-rs485.o
	$(CC) $(CFLAGS) -o bpi-rs485 bpi-rs485.o

can-bounce-test: can-bounce-test.o
	$(CC) $(CFLAGS) -o can-bounce-test can-bounce-test.o

can-monitor: can-monitor.o
	$(CC) $(CFLAGS) -o can-monitor can-monitor.o -lpcap

can2eth: can2eth.o
	$(CC) $(CFLAGS) -o can2eth can2eth.o

lan-schnitte: lan-schnitte.o
	$(CC) $(CFLAGS) -o lan-schnitte lan-schnitte.o

can2udp: can2udp.o
	$(CC) $(CFLAGS) -o can2udp can2udp.o

can2serial: can2serial.o
	$(CC) $(CFLAGS) -o can2serial can2serial.o

cs2ping: cs2ping.o
	$(CC) $(CFLAGS) -o cs2ping cs2ping.o

can4lnx2udp: can4lnx2udp.o
	$(CC) $(CFLAGS) -o can4lnx2udp can4lnx2udp.o

read-cs2-ini: read-cs2-config.o read-cs2-ini.o
	$(CC) $(CFLAGS) -o read-cs2-ini read-cs2-config.o read-cs2-ini.o

clone-cs2-config: crc-ccitt.o clone-cs2-config.o
	$(CC) $(CFLAGS) -o clone-cs2-config crc-ccitt.o clone-cs2-config.o -lz

clone-cs2-trigger: clone-cs2-trigger.o
	$(CC) $(CFLAGS) -o clone-cs2-trigger clone-cs2-trigger.o -lpthread

clone-ms2-loco: crc-ccitt.o read-cs2-config.o clone-ms2-loco.o
	$(CC) $(CFLAGS) -o clone-ms2-loco crc-ccitt.o read-cs2-config.o clone-ms2-loco.o -lpthread

gb2-update: crc-ccitt.o gb2-update.o
	$(CC) $(CFLAGS) -o gb2-update crc-ccitt.o gb2-update.o

scan2lan: crc-ccitt.o sgio.o scan2lan.o
	$(CC) $(CFLAGS) -o scan2lan crc-ccitt.o sgio.o scan2lan.o -lz

can2lan: crc-ccitt.o gio.o can2lan.o
	$(CC) $(CFLAGS) -o can2lan crc-ccitt.o gio.o can2lan.o -lz

ms2wifi: ms2wifi.o
	$(CC) $(CFLAGS) -o ms2wifi ms2wifi.o

virtual_canbuster: virtual_canbuster.o
	$(CC) $(CFLAGS) -o virtual_canbuster virtual_canbuster.o

virtual_gfp: virtual_gfp.o
	$(CC) $(CFLAGS) -o virtual_gfp virtual_gfp.o

virtual_gfp3: virtual_gfp3.o
	$(CC) $(CFLAGS) -o virtual_gfp3 virtual_gfp3.o

virtual_links88: virtual_links88.o
	$(CC) $(CFLAGS) -o virtual_links88 virtual_links88.o

virtual_ms2: virtual_ms2.o
	$(CC) $(CFLAGS) -o virtual_ms2 virtual_ms2.o

get-cs-data: get-cs-data.o
	$(CC) $(CFLAGS) -o get-cs-data get-cs-data.o

ir-tester: ir-tester.o
	$(CC) $(CFLAGS) -o ir-tester ir-tester.o

get-cs-config: crc-ccitt.o get-cs-config.o
	$(CC) $(CFLAGS) -o get-cs-config crc-ccitt.o get-cs-config.o -lz

pio-i2c-udp: pio-i2c-udp.o
	$(CC) $(CFLAGS) -o pio-i2c-udp pio-i2c-udp.o

read_lococard: read_lococard.o
	$(CC) $(CFLAGS) -o read_lococard read_lococard.o

s88udp-bpi: allwinner.o s88udp-bpi.o
	$(CC) $(CFLAGS) -o s88udp-bpi allwinner.o s88udp-bpi.o

s88udp-rpi: raspi.o s88udp-rpi.o
	$(CC) $(CFLAGS) -o s88udp-rpi raspi.o s88udp-rpi.o

s88test-bpi: allwinner.o s88test-bpi.o
	$(CC) $(CFLAGS) -o s88test-bpi allwinner.o s88test-bpi.o

wake-up-links88: wake-up-links88.o
	$(CC) $(CFLAGS) -o wake-up-links88 wake-up-links88.o

virtual_s88udp: virtual_s88udp.o
	$(CC) $(CFLAGS) -o virtual_s88udp virtual_s88udp.o

gpio-poll: gpio-poll.o
	$(CC) $(CFLAGS) -o gpio-poll gpio-poll.o

xntcpd: xntcpd.o
	$(CC) $(CFLAGS) -o xntcpd xntcpd.o

s88can: s88can.o mcan.o allwinner.o
	$(CC) $(CFLAGS) -o s88can allwinner.o mcan.o s88can.o

canpanel: canpanel.o mcan.o
	$(CC) $(CFLAGS) -o canpanel mcan.o canpanel.o

clean:
	$(RM) *.o $(BINS) s88udp
