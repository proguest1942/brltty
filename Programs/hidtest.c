/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2021 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option) any
 * later version. Please see the file LICENSE-LGPL for details.
 *
 * Web Page: http://brltty.app/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include "prologue.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "program.h"
#include "options.h"
#include "log.h"
#include "strfmt.h"
#include "parse.h"
#include "io_hid.h"
#include "hid_items.h"
#include "hid_inspect.h"

static int opt_forceUSB;
static int opt_forceBluetooth;

static char *opt_vendorIdentifier;
static char *opt_productIdentifier;

static char *opt_manufacturerName;
static char *opt_productDescription;
static char *opt_serialNumber;

static char *opt_macAddress;
static char *opt_deviceName;

static int opt_showIdentifiers;
static int opt_showDeviceIdentifier;
static int opt_showDeviceName;
static int opt_showHostPath;
static int opt_showHostDevice;
static int opt_listItems;

static char *opt_readReport;
static char *opt_readFeature;
static char *opt_writeReport;
static char *opt_writeFeature;

static int opt_echoInput;
static char *opt_inputTimeout;

static const char *parseBytesHelp[] = {
  strtext("Bytes may be separated by whitespace."),
  strtext("Each byte is either two hexadecimal digits or [zero or more braille dot numbers within brackets]."),
  strtext("A byte may optionally be followed by an asterisk [*] and a decimal count (1 if not specified)."),
  strtext("The first byte is the report number (00 for no report number)."),
};

static
STR_BEGIN_FORMATTER(formatParseBytesHelp, unsigned int index)
  switch (index) {
    case 0: {
      const char *const *sentence = parseBytesHelp;
      const char *const *end = sentence + ARRAY_COUNT(parseBytesHelp);

      while (sentence < end) {
        if (STR_LENGTH) STR_PRINTF(" ");
        STR_PRINTF("%s", gettext(*sentence));
        sentence += 1;
      }

      break;
    }

    default:
      break;
  }
STR_END_FORMATTER

BEGIN_OPTION_TABLE(programOptions)
  { .word = "usb",
    .letter = 'u',
    .setting.flag = &opt_forceUSB,
    .description = strtext("Filter for a USB device (the default if not ambiguous).")
  },

  { .word = "bluetooth",
    .letter = 'b',
    .setting.flag = &opt_forceBluetooth,
    .description = strtext("Filter for a Bluetooth device.")
  },

  { .word = "vendor",
    .letter = 'v',
    .argument = strtext("identifier"),
    .setting.string = &opt_vendorIdentifier,
    .description = strtext("Match the vendor identifier (four hexadecimal digits).")
  },

  { .word = "product",
    .letter = 'p',
    .argument = strtext("identifier"),
    .setting.string = &opt_productIdentifier,
    .description = strtext("Match the product identifier (four hexadecimal digits).")
  },

  { .word = "manufacturer",
    .letter = 'm',
    .argument = strtext("string"),
    .setting.string = &opt_manufacturerName,
    .description = strtext("Match the start of the manufacturer name (USB only).")
  },

  { .word = "description",
    .letter = 'd',
    .argument = strtext("string"),
    .setting.string = &opt_productDescription,
    .description = strtext("Match the start of the product description (USB only).")
  },

  { .word = "serial-number",
    .letter = 's',
    .argument = strtext("string"),
    .setting.string = &opt_serialNumber,
    .description = strtext("Match the start of the serial number (USB only).")
  },

  { .word = "address",
    .letter = 'a',
    .argument = strtext("octets"),
    .setting.string = &opt_macAddress,
    .description = strtext("Match the full MAC address (Bluetooth only - all six two-digit, hexadecimal octets separated by a colon [:]).")
  },

  { .word = "name",
    .letter = 'n',
    .argument = strtext("string"),
    .setting.string = &opt_deviceName,
    .description = strtext("Match the start of the device name (Bluetooth only).")
  },

  { .word = "identifiers",
    .letter = 'i',
    .setting.flag = &opt_showIdentifiers,
    .description = strtext("Show the vendor and product identifiers.")
  },

  { .word = "device-identifier",
    .letter = 'I',
    .setting.flag = &opt_showDeviceIdentifier,
    .description = strtext("Show the device identifier (USB serial number, Bluetooth device address, etc).")
  },

  { .word = "device-name",
    .letter = 'N',
    .setting.flag = &opt_showDeviceName,
    .description = strtext("Show the device name (USB manufacturer and/or product strings, Bluetooth device name, etc).")
  },

  { .word = "host-path",
    .letter = 'P',
    .setting.flag = &opt_showHostPath,
    .description = strtext("Show the host path (USB topology, Bluetooth host controller address, etc).")
  },

  { .word = "host-device",
    .letter = 'D',
    .setting.flag = &opt_showHostDevice,
    .description = strtext("Show the host device (usually its absolute path).")
  },

  { .word = "list",
    .letter = 'l',
    .setting.flag = &opt_listItems,
    .description = strtext("List the HID report descriptor's items.")
  },

  { .word = "read-report",
    .letter = 'r',
    .argument = strtext("number"),
    .setting.string = &opt_readReport,
    .description = strtext("Read (get) an input report (a decimal integer from 0 through 255).")
  },

  { .word = "read-feature",
    .letter = 'R',
    .argument = strtext("number"),
    .setting.string = &opt_readFeature,
    .description = strtext("Read (get) a feature report (a decimal integer from 1 through 255).")
  },

  { .word = "write-report",
    .letter = 'w',
    .argument = strtext("bytes"),
    .setting.string = &opt_writeReport,
    .flags = OPT_Format,
    .description = strtext("Write (set) an output report. %s"),
    .strings.format = formatParseBytesHelp
  },

  { .word = "write-feature",
    .letter = 'W',
    .argument = strtext("bytes"),
    .setting.string = &opt_writeFeature,
    .flags = OPT_Format,
    .description = strtext("Write (set) a feature report. %s"),
    .strings.format = formatParseBytesHelp
  },

  { .word = "echo",
    .letter = 'e',
    .setting.flag = &opt_echoInput,
    .description = strtext("Echo (in hexadecimal) input received from the device.")
  },

  { .word = "timeout",
    .letter = 't',
    .argument = strtext("integer"),
    .setting.string = &opt_inputTimeout,
    .description = strtext("The input timeout (in seconds).")
  },
END_OPTION_TABLE

static FILE *outputStream;
static int outputError;

static int
canWriteOutput (void) {
  if (outputError) return 0;
  if (!ferror(outputStream)) return 1;

  outputError = errno;
  return 0;
}

static int writeBytesLine (
  const char *format,
  const unsigned char *from, size_t count,
  ...
) PRINTF(1, 4);

static int
writeBytesLine (const char *format, const unsigned char *from, size_t count, ...) {
  const unsigned char *to = from + count;

  char bytes[(count * 3) + 1];
  STR_BEGIN(bytes, sizeof(bytes));

  while (from < to) {
    STR_PRINTF(" %02X", *from++);
  }
  STR_END;

  char label[0X100];
  {
    va_list arguments;
    va_start(arguments, count);
    vsnprintf(label, sizeof(label), format, arguments);
    va_end(arguments);
  }

  fprintf(outputStream, "%s:%s\n", label, bytes);
  if (!canWriteOutput()) return 0;

  fflush(outputStream);
  return canWriteOutput();
}

static int
parseString (const char *value, void *field) {
  const char **string = field;
  *string = value;
  return 1;
}

static int
parseIdentifier (const char *value, void *field) {
  uint16_t *identifier = field;
  return hidParseIdentifier(identifier, value);
}

static int
parseMacAddress (const char *value, void *field) {
  {
    const char *byte = value;
    unsigned int state = 0;
    unsigned int octets = 0;
    const char *digits = "0123456789ABCDEFabcdef";

    while (*byte) {
      if (!state) octets += 1;

      if (++state < 3) {
        if (!strchr(digits, *byte)) return 0;
      } else {
        if (*byte != ':') return 0;
        state = 0;
      }

      byte += 1;
    }

    if (octets != 6) return 0;
    if (state != 2) return 0;
  }

  return parseString(value, field);
}

static int
openDevice (HidDevice **device) {
  HidUSBFilter huf;
  hidInitializeUSBFilter(&huf);

  HidBluetoothFilter hbf;
  hidInitializeBluetoothFilter(&hbf);

  typedef struct {
    const char *name;
    const char *value;
    void *field;
    int (*parser) (const char *value, void *field);
    int *flag;
  } FilterEntry;

  const FilterEntry filterTable[] = {
    { .name = "vendor identifier",
      .value = opt_vendorIdentifier,
      .field = &huf.vendorIdentifier,
      .parser = parseIdentifier,
    },

    { .name = "product identifier",
      .value = opt_productIdentifier,
      .field = &huf.productIdentifier,
      .parser = parseIdentifier,
    },

    { .name = "manufacturer name",
      .value = opt_manufacturerName,
      .field = &huf.manufacturerName,
      .parser = parseString,
      .flag = &opt_forceUSB,
    },

    { .name = "product description",
      .value = opt_productDescription,
      .field = &huf.productDescription,
      .parser = parseString,
      .flag = &opt_forceUSB,
    },

    { .name = "serial number",
      .value = opt_serialNumber,
      .field = &huf.serialNumber,
      .parser = parseString,
      .flag = &opt_forceUSB,
    },

    { .name = "MAC address",
      .value = opt_macAddress,
      .field = &hbf.macAddress,
      .parser = parseMacAddress,
      .flag = &opt_forceBluetooth,
    },

    { .name = "device name",
      .value = opt_deviceName,
      .field = &hbf.deviceName,
      .parser = parseString,
      .flag = &opt_forceBluetooth,
    },
  };

  const FilterEntry *filter = filterTable;
  const FilterEntry *end = filter + ARRAY_COUNT(filterTable);

  while (filter < end) {
    if (filter->value && *filter->value) {
      if (!filter->parser(filter->value, filter->field)) {
        logMessage(LOG_ERR, "invalid %s: %s", filter->name, filter->value);
        return 0;
      }

      if (filter->flag) {
        *filter->flag = 1;

        if (opt_forceUSB && opt_forceBluetooth) {
          logMessage(LOG_ERR, "conflicting filter options");
          return 0;
        }
      }
    }

    filter += 1;
  }

  hbf.vendorIdentifier = huf.vendorIdentifier;
  hbf.productIdentifier = huf.productIdentifier;

  if (opt_forceBluetooth) {
    *device = hidOpenBluetoothDevice(&hbf);
  } else {
    *device = hidOpenUSBDevice(&huf);
  }

  return 1;
}

static int
getReportSize (HidDevice *device, unsigned char identifier, HidReportSize *size) {
  const HidItemsDescriptor *items = hidGetItems(device);
  if (!items) return 0;
  return hidGetReportSize(items, identifier, size);
}

static int
performShowIdentifiers (HidDevice *device) {
  uint16_t vendor;
  uint16_t product;

  if (!hidGetIdentifiers(device, &vendor, &product)) {
    logMessage(LOG_WARNING, "vendor/product identifiers not available");
    return 0;
  }

  fprintf(outputStream,
    "Vendor Identifier: %04X\n"
    "Product Identifier: %04X\n",
    vendor, product
  );

  return 1;
}

static int
performShowDeviceIdentifier (HidDevice *device) {
  const char *identifier = hidGetDeviceIdentifier(device);

  if (!identifier) {
    logMessage(LOG_WARNING, "device identifier not available");
    return 0;
  }

  fprintf(outputStream, "Device Identifier: %s\n", identifier);
  return 1;
}

static int
performShowDeviceName (HidDevice *device) {
  const char *name = hidGetDeviceName(device);

  if (!name) {
    logMessage(LOG_WARNING, "device name not available");
    return 0;
  }

  fprintf(outputStream, "Device Name: %s\n", name);
  return 1;
}

static int
performShowHostPath (HidDevice *device) {
  const char *path = hidGetHostPath(device);

  if (!path) {
    logMessage(LOG_WARNING, "host path not available");
    return 0;
  }

  fprintf(outputStream, "Host Path: %s\n", path);
  return 1;
}

static int
performShowHostDevice (HidDevice *device) {
  const char *hostDevice = hidGetHostDevice(device);

  if (!hostDevice) {
    logMessage(LOG_WARNING, "host device not available");
    return 0;
  }

  fprintf(outputStream, "Host Device: %s\n", hostDevice);
  return 1;
}

static int
listItem (const char *line, void *data) {
  fprintf(outputStream, "%s\n", line);
  return canWriteOutput();
}

static int
performListItems (HidDevice *device) {
  const HidItemsDescriptor *items = hidGetItems(device);
  if (!items) return 0;

  hidListItems(items, listItem, NULL);
  return 1;
}

static int
isReportNumber (unsigned char *number, const char *string, unsigned char minimum) {
  unsigned int value;

  if (isUnsignedInteger(&value, string)) {
    if ((value >= minimum) && (value < 0X100)) {
      *number = value;
      return 1;
    }
  }

  return 0;
}

static unsigned char readReportNumber;

static int
parseReadReport (void) {
  const char *number = opt_readReport;
  if (!*number) return 1;
  if (isReportNumber(&readReportNumber, number, 0)) return 1;

  logMessage(LOG_ERR, "invalid input report number: %s", number);
  return 0;
}

static int
performReadReport (HidDevice *device) {
  if (!*opt_readReport) return 1;
  HidReportSize reportSize;

  if (getReportSize(device, readReportNumber, &reportSize)) {
    size_t size = reportSize.input - 1;

    if (size) {
      unsigned char report[size];
      report[0] = readReportNumber;

      if (!hidGetReport(device, report, size)) return 0;
      writeBytesLine("Input Report: %02X", report, size, readReportNumber);
      return 1;
    }
  }

  logMessage(LOG_ERR, "input report not defined: %u", readReportNumber);
  return 0;
}

static unsigned char readFeatureNumber;

static int
parseReadFeature (void) {
  const char *number = opt_readFeature;
  if (!*number) return 1;
  if (isReportNumber(&readFeatureNumber, number, 1)) return 1;

  logMessage(LOG_ERR, "invalid feature report number: %s", number);
  return 0;
}

static int
performReadFeature (HidDevice *device) {
  if (!*opt_readFeature) return 1;
  HidReportSize reportSize;

  if (getReportSize(device, readFeatureNumber, &reportSize)) {
    size_t size = reportSize.feature - 1;

    if (size) {
      unsigned char feature[size];
      feature[0] = readFeatureNumber;

      if (!hidGetFeature(device, feature, size)) return 0;
      writeBytesLine("Feature Report: %02X", feature, size, readFeatureNumber);
      return 1;
    }
  }

  logMessage(LOG_ERR, "feature report not defined: %u", readFeatureNumber);
  return 0;
}

static int
isHexadecimal (unsigned char *digit, char character) {
  const char string[] = {character, 0};
  char *end;
  long int value = strtol(string, &end, 0X10);

  if (*end) {
    logMessage(LOG_ERR, "invalid hexadecimal digit: %c", character);
    return 0;
  }

  *digit = value;
  return 1;
}

static int
parseBytes (
  const char *bytes, const char *what, unsigned char *buffer,
  size_t bufferSize, size_t *bufferUsed
) {
  unsigned char *out = buffer;
  const unsigned char *end = out + bufferSize;

  if (*bytes) {
    const char *in = bytes;
    unsigned char byte = 0;
    unsigned int count = 1;

    enum {NEXT, HEX, DOTS, COUNT};
    unsigned int state = NEXT;

    while (*in) {
      char character = *in++;

      switch (state) {
        case NEXT: {
          if (iswspace(character)) continue;

          if (character == '[') {
            state = DOTS;
            continue;
          }

          unsigned char digit;
          if (!isHexadecimal(&digit, character)) return 0;

          byte = digit << 4;
          state = HEX;
          continue;
        }

        case HEX: {
          unsigned char digit;
          if (!isHexadecimal(&digit, character)) return 0;

          byte |= digit;
          state = NEXT;
          break;
        }

        case DOTS: {
          if (character == ']') {
            state = NEXT;
            break;
          }

          if ((character < '1') || (character > '8')) {
            logMessage(LOG_ERR, "invalid dot number: %c", character);
            return 0;
          }
          unsigned char bit = 1 << (character - '1');

          if (byte & bit) {
            logMessage(LOG_ERR, "duplicate dot number: %c", character);
            return 0;
          }

          byte |= bit;
          continue;
        }

        case COUNT: {
          if (iswspace(character)) break;
          int digit = character - '0';

          if ((digit < 0) || (digit > 9)) {
            logMessage(LOG_ERR, "invalid count digit: %c", character);
            return 0;
          }

          if (!digit) {
            if (!count) {
              logMessage(LOG_ERR, "first digit of count can't be 0");
              return 0;
            }
          }

          count *= 10;
          count += digit;

          if (!*in) break;
          continue;
        }

        default:
          logMessage(LOG_ERR, "unexpected bytes parser state: %u", state);
          return 0;
      }

      if (state == COUNT) {
        if (!count) {
          logMessage(LOG_ERR, "missing count");
          return 0;
        }

        state = NEXT;
      } else if (*in == '*') {
        in += 1;
        state = COUNT;
        count = 0;
        continue;
      }

      while (count--) {
        if (out == end) {
          logMessage(LOG_ERR, "%s buffer too small", what);
          return 0;
        }

        *out++ = byte;
      }

      byte = 0;
      count = 1;
    }

    if (state != NEXT) {
      logMessage(LOG_ERR, "incomplete %s specification", what);
      return 0;
    }
  }

  *bufferUsed = out - buffer;
  return 1;
}

static int
verifyWrite (
  HidDevice *device, const char *what,
  HidReportSize *reportSize, size_t *expectedSize,
  const unsigned char *buffer, size_t bufferSize
) {
  logBytes(LOG_NOTICE, "writing %s report", buffer, bufferSize, what);

  {
    unsigned char identifier = buffer[0];
    int isDefined = 0;

    if (getReportSize(device, identifier, reportSize)) {
      if (*expectedSize) {
        isDefined = 1;
      }
    }

    if (!isDefined) {
      logMessage(LOG_ERR, "%s report not defined: %02X", what, identifier);
      return 0;
    }

    if (!identifier) *expectedSize += 1;
    size_t actualSize = bufferSize;

    if (actualSize != *expectedSize) {
      logMessage(LOG_ERR,
        "incorrect %s report size: %02X:"
        " Expected:%"PRIsize " Actual:%"PRIsize,
        what, identifier, *expectedSize, actualSize
      );

      return 0;
    }
  }

  return 1;
}

static unsigned char writeReportBuffer[0X1000];
static size_t writeReportLength;

static int
parseWriteReport (void) {
  return parseBytes(
    opt_writeReport, "output report", writeReportBuffer,
    ARRAY_COUNT(writeReportBuffer), &writeReportLength
  );
}

static int
performWriteReport (HidDevice *device) {
  if (!writeReportLength) return 1;
  HidReportSize reportSize;

  int verified = verifyWrite(
    device, "output",
    &reportSize, &reportSize.output,
    writeReportBuffer, writeReportLength
  );

  if (!verified) return 0;
  return hidSetReport(device, writeReportBuffer, writeReportLength);
}

static unsigned char writeFeatureBuffer[0X1000];
static size_t writeFeatureLength;

static int
parseWriteFeature (void) {
  return parseBytes(
    opt_writeFeature, "feature report", writeFeatureBuffer,
    ARRAY_COUNT(writeFeatureBuffer), &writeFeatureLength
  );
}

static int
performWriteFeature (HidDevice *device) {
  if (!writeFeatureLength) return 1;
  HidReportSize reportSize;

  int verified = verifyWrite(
    device, "feature",
    &reportSize, &reportSize.feature,
    writeFeatureBuffer, writeFeatureLength
  );

  if (!verified) return 0;
  return hidSetFeature(device, writeFeatureBuffer, writeFeatureLength);
}

static int inputTimeout;

static int
parseInputTimeout (void) {
  inputTimeout = 10;

  static const int minimum = 1;
  static const int maximum = 99;

  if (!validateInteger(&inputTimeout, opt_inputTimeout, &minimum, &maximum)) {
    logMessage(LOG_ERR, "invalid input timeout: %s", opt_inputTimeout);
    return 0;
  }

  inputTimeout *= 1000;
  return 1;
}

static int
performEchoInput (HidDevice *device) {
  HidReportSize reportSize;
  const size_t *inputSize = &reportSize.input;

  unsigned char reportIdentifier = 0;
  int hasReportIdentifiers = !getReportSize(device, reportIdentifier, &reportSize);

  unsigned char buffer[0X1000];
  size_t bufferSize = sizeof(buffer);

  unsigned char *from = buffer;
  unsigned char *to = from;
  const unsigned char *end = from + bufferSize;

  while (hidAwaitInput(device, inputTimeout)) {
    ssize_t result = hidReadData(device, to, (end - to), 1000, 100);

    if (result == -1) {
      logMessage(LOG_ERR, "input error: %s", strerror(errno));
      return 0;
    }

    to += result;

    while (from < to) {
      if (hasReportIdentifiers) {
        reportIdentifier = *from;

        if (!getReportSize(device, reportIdentifier, &reportSize)) {
          logMessage(LOG_ERR, "input report not defined: %02X", reportIdentifier);
          return 0;
        }
      }

      if (!*inputSize) {
        logMessage(LOG_ERR, "input report size is zero: %02X", reportIdentifier);
        return 0;
      }

      size_t count = to - from;

      if (*inputSize > count) {
        if (from == buffer) {
          logMessage(LOG_ERR,
            "input report too large: %02X: %"PRIsize " > %"PRIsize,
            reportIdentifier, *inputSize, count
          );

          return 0;
        }

        memmove(buffer, from, count);
        from = buffer;
        to = from + count;

        break;
      }

      if (!writeBytesLine("Input Report", from, *inputSize)) return 0;
      from += *inputSize;
    }
  }

  return 1;
}

static int
parseOperands (void) {
  typedef struct {
    int (*parse) (void);
  } OperandEntry;

  static const OperandEntry operandTable[] = {
    { .parse = parseReadReport,
    },

    { .parse = parseReadFeature,
    },

    { .parse = parseWriteReport,
    },

    { .parse = parseWriteFeature,
    },

    { .parse = parseInputTimeout,
    },
  };

  const OperandEntry *operand = operandTable;
  const OperandEntry *end = operand + ARRAY_COUNT(operandTable);

  while (operand < end) {
    if (!operand->parse()) return 0;
    operand += 1;
  }

  return 1;
}

static int
performActions (HidDevice *device) {
  typedef struct {
    int (*perform) (HidDevice *device);
    int *requested;
  } ActionEntry;

  static const ActionEntry actionTable[] = {
    { .perform = performShowIdentifiers,
      .requested = &opt_showIdentifiers,
    },

    { .perform = performShowDeviceIdentifier,
      .requested = &opt_showDeviceIdentifier,
    },

    { .perform = performShowDeviceName,
      .requested = &opt_showDeviceName,
    },

    { .perform = performShowHostPath,
      .requested = &opt_showHostPath,
    },

    { .perform = performShowHostDevice,
      .requested = &opt_showHostDevice,
    },

    { .perform = performListItems,
      .requested = &opt_listItems,
    },

    { .perform = performReadReport,
    },

    { .perform = performReadFeature,
    },

    { .perform = performWriteReport,
    },

    { .perform = performWriteFeature,
    },

    { .perform = performEchoInput,
      .requested = &opt_echoInput,
    },
  };

  const ActionEntry *action = actionTable;
  const ActionEntry *end = action + ARRAY_COUNT(actionTable);

  while (action < end) {
    if (!action->requested || *action->requested) {
      if (!action->perform(device)) return 0;
      if (!canWriteOutput()) return 0;
    }

    action += 1;
  }

  return 1;
}

int
main (int argc, char *argv[]) {
  {
    static const OptionsDescriptor descriptor = {
      OPTION_TABLE(programOptions),
      .applicationName = "hidtest",
    };

    PROCESS_OPTIONS(descriptor, argc, argv);
  }

  outputStream = stdout;
  outputError = 0;

  if (argc) {
    logMessage(LOG_ERR, "too many parameters");
    return PROG_EXIT_SYNTAX;
  }

  if (!parseOperands()) return PROG_EXIT_SYNTAX;
  ProgramExitStatus exitStatus = PROG_EXIT_SUCCESS;
  HidDevice *device = NULL;

  if (!openDevice(&device)) {
    exitStatus = PROG_EXIT_SYNTAX;
  } else if (!device) {
    logMessage(LOG_ERR, "device not found");
    exitStatus = PROG_EXIT_SEMANTIC;
  } else {
    if (!performActions(device)) exitStatus = PROG_EXIT_FATAL;
    hidCloseDevice(device);
  }

  if (outputError) {
    logMessage(LOG_ERR, "output error: %s", strerror(outputError));
    exitStatus = PROG_EXIT_FATAL;
  }

  return exitStatus;
}
