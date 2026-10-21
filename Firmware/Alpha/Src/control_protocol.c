#include "control_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static control_command parse(char *line)
{
    control_command cmd = { CONTROL_ERROR, 0, 0, 0 };
    char *words[4], *p = line, *end;
    unsigned count = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        if (count == 4) return cmd;
        words[count++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            *p = (char)toupper((unsigned char)*p); ++p;
        }
        if (*p) *p++ = 0;
    }
    if (!count) return cmd;
    if (count == 1) {
        static const char *names[] = {"HELP", "INFO", "STATUS", "STOP", "START", "REBOOT", "BOOTLOADER",
                                      "DIAG", "SETTINGS", "SAVE", "LOAD", "FACTORY", "CHECK"};
        static const control_kind kinds[] = {CONTROL_HELP, CONTROL_INFO, CONTROL_STATUS,
                                            CONTROL_STOP, CONTROL_START, CONTROL_REBOOT, CONTROL_BOOTLOADER,
                                            CONTROL_DIAG, CONTROL_SETTINGS, CONTROL_SAVE, CONTROL_LOAD, CONTROL_FACTORY, CONTROL_CHECK};
        unsigned i;
        for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i)
            if (!strcmp(words[0], names[i])) { cmd.kind = kinds[i]; return cmd; }
    }
    if (count == 2 && (!strcmp(words[0], "HELP") || !strcmp(words[0], "PRESET"))) {
        static const char *topics[] = {"SETTINGS", "PRESETS", "DIAG"};
        static const char *presets[] = {"DEFAULT", "FAST", "LOWGAIN"};
        unsigned i, help = !strcmp(words[0], "HELP");
        for (i = 0; i < 3; ++i) if (!strcmp(words[1], help ? topics[i] : presets[i])) {
            cmd.kind = help ? CONTROL_HELP : CONTROL_PRESET; cmd.first = i + 1; return cmd;
        }
        return cmd;
    }
    if ((count == 2 && !strcmp(words[0], "RATE")) ||
        (count == 3 && (!strcmp(words[0], "GAIN") || !strcmp(words[0], "SPIREAD"))) ||
        (count == 4 && !strcmp(words[0], "SPIWRITE"))) {
        unsigned i;
        uint32_t values[3] = {0, 0, 0};
        for (i = 1; i < count; ++i) {
            /* Explicitly reject signs, suffixes and overflow before strtoul. */
            const char *q = words[i];
            if (!*q || strlen(q) > 6) return cmd;
            while (*q) if (!isdigit((unsigned char)*q++)) return cmd;
            values[i - 1] = (uint32_t)strtoul(words[i], &end, 10);
            if (*end) return cmd;
        }
        cmd.first = values[0]; cmd.second = values[1]; cmd.third = values[2];
        if (count == 2 && (cmd.first == 250 || cmd.first == 500 || cmd.first == 1000 || cmd.first == 2000))
            cmd.kind = CONTROL_RATE;
        if (count == 3 && !strcmp(words[0], "GAIN") && cmd.first >= 1 && cmd.first <= 128 &&
            (cmd.second == 1 || cmd.second == 2 || cmd.second == 4 || cmd.second == 8 || cmd.second == 16 || cmd.second == 32 || cmd.second == 64 || cmd.second == 128))
            cmd.kind = CONTROL_GAIN;
        if (count == 3 && !strcmp(words[0], "SPIREAD") && cmd.first >= 1 && cmd.first <= 16 &&
            cmd.second <= 62)
            cmd.kind = CONTROL_SPIREAD;
    }
    return cmd;
}

control_command control_feed(control_parser *parser, uint8_t byte)
{
    control_command cmd = { CONTROL_NONE, 0, 0, 0 };
    if (byte == '\r' || byte == '\n') {
        if (parser->used && parser->line[0] == '!') {
            parser->line[parser->used] = 0;
            cmd = parser->overflow ? (control_command){ CONTROL_ERROR, 0, 0, 0 }
                                   : parse(parser->line + 1);
        }
        parser->used = 0; parser->overflow = 0;
    } else if (!parser->overflow) {
        if (byte < 32 || byte > 126) parser->overflow = 1;
        else if (parser->used < CONTROL_LINE_SIZE - 1) parser->line[parser->used++] = (char)byte;
        else parser->overflow = 1;
    }
    return cmd;
}
