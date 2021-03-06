/*
 * This file is part of the IOTA Access distribution
 * (https://github.com/iotaledger/access)
 *
 * Copyright (c) 2020 IOTA Stiftung
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/****************************************************************************
 * \project IOTA Access
 * \file modbus.h
 * \brief
 * Modbus RTU interface implementation
 *
 * @Author Djordje Golubovic
 *
 * \notes
 *
 * \history
 * 07.26.2019. Initial version.
 ****************************************************************************/

#include "modbus.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MODBUS_DEVICE_NAME_LEN 127
#define MODBUS_SLAVE_DEVICE_BUFF_LEN 8
#define MODBUS_READ_REG_FN 0x03
#define MODBUS_READ_BUFF_LEN 100
#define MODBUS_DATA_LEN 256
#define MODBUS_0_5S_TIMEOUT 5
#define MODBUS_CRC_SHIFT 8
#define MODBUS_CRC_AND_MASK 0x0001
#define MODBUS_CRC_XOR_MASK 0xA001

static int set_interface_attribs(int fd, int speed, int parity);
static void set_blocking(int fd, int should_block);
static uint16_t calculate_crc(uint8_t* buf, int len);

int modbus_init(modbus_t* modbus, const char* serial_device) {
  modbus->fd = -1;
  strncpy(modbus->device_name, serial_device, MODBUS_DEVICE_NAME_LEN);
  modbus->fd = open(serial_device, O_RDWR | O_NOCTTY | O_SYNC);

  if (modbus->fd < 0) {
    printf("Couldn't open serial port at %s\n", serial_device);
    return -1;
  }

  set_interface_attribs(modbus->fd, B115200, 0);  // set speed to 115200 bps, 8n1 (no parity)
  set_blocking(modbus->fd, 1);                    // set blocking
}

int modbus_read_registers(modbus_t* modbus, int slave_device_address, uint16_t register_address,
                          uint16_t quantity_to_read, int16_t* data) {
  uint8_t buff[MODBUS_SLAVE_DEVICE_BUFF_LEN] = {0};
  buff[0] = slave_device_address;
  buff[1] = MODBUS_READ_REG_FN;
  buff[2] = ((uint8_t*)(&register_address))[1];
  buff[3] = ((uint8_t*)(&register_address))[0];
  buff[4] = ((uint8_t*)(&quantity_to_read))[1];
  buff[5] = ((uint8_t*)(&quantity_to_read))[0];

  uint16_t crc = calculate_crc(buff, 6);

  buff[6] = ((uint8_t*)(&crc))[0];
  buff[7] = ((uint8_t*)(&crc))[1];

  ssize_t wsize = write(modbus->fd, buff, MODBUS_SLAVE_DEVICE_BUFF_LEN);
  uint8_t buffer[MODBUS_READ_BUFF_LEN] = {0};
  ssize_t length = read(modbus->fd, &buffer, MODBUS_READ_BUFF_LEN);

  int bytes_read = buffer[2];
  int response_len = 3 + bytes_read;  // length, not including 2 bytes of CRC

  crc = calculate_crc(&buffer[0], response_len);
  uint16_t response_crc = *((uint16_t*)(&buffer[response_len]));

  uint8_t* start_of_registers = &buffer[3];

  for (int i = 0; i < bytes_read / 2; i++) {
    data[i] = start_of_registers[i * 2 + 1] + start_of_registers[i * 2] * MODBUS_DATA_LEN;
  }

  return response_crc != crc;
}

void modbus_deinit(modbus_t* modbus) { close(modbus->fd); }

static int set_interface_attribs(int fd, int speed, int parity) {
  struct termios tty;
  memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd, &tty) != 0) {
    fprintf(stderr, "error %d from tcgetattr", errno);
    return -1;
  }

  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
  // disable IGNBRK for mismatched speed tests; otherwise receive break
  // as \000 chars
  tty.c_iflag &= ~IGNBRK;                 // disable break processing
  tty.c_lflag = 0;                        // no signaling chars, no echo,
                                          // no canonical processing
  tty.c_oflag = 0;                        // no remapping, no delays
  tty.c_cc[VMIN] = 0;                     // read doesn't block
  tty.c_cc[VTIME] = MODBUS_0_5S_TIMEOUT;  // 0.5 seconds read timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);    // ignore modem controls,
                                      // enable reading
  tty.c_cflag &= ~(PARENB | PARODD);  // shut off parity
  tty.c_cflag |= parity;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    fprintf(stderr, "error %d from tcsetattr", errno);
    return -1;
  }
  return 0;
}

static void set_blocking(int fd, int should_block) {
  struct termios tty;
  memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd, &tty) != 0) {
    fprintf(stderr, "error %d from tggetattr", errno);
    return;
  }

  tty.c_cc[VMIN] = should_block ? 1 : 0;
  tty.c_cc[VTIME] = MODBUS_0_5S_TIMEOUT;  // 0.5 seconds read timeout

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    fprintf(stderr, "error %d setting term attributes", errno);
  }
}

static uint16_t calculate_crc(uint8_t* buf, int len) {
  uint16_t crc = 0xffff;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];

    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}
