/*
 * Copyright (c) 2013 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "websocket.h"

static char rn[] PROGMEM = "\r\n";
void show4bits(char c)
{
    char outstr[5]; // the binary string that will contain the 1's and 0's
    short b = (short)c;
    int i;
    for (i = 3; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[4]='\0';
    printf("%s\n",outstr);
}

void showuint8(uint8_t b)
{
    char outstr[9]; // the binary string that will contain the 1's and 0's
    int i;
    for (i = 7; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[8]='\0';
    printf("%s\n",outstr);
}

void showuint16(uint16_t b)
{
    char outstr[17]; // the binary string that will contain the 1's and 0's
    int i;
    for (i = 15; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[16]='\0';
    printf("%s\n",outstr);
}

void showuint32(uint32_t b)
{
    char outstr[33]; // the binary string that will contain the 1's and 0's
    int i;
    for (i = 31; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[32]='\0';
    printf("%s",outstr);
}

void showuint64(uint64_t b)
{
    char outstr[65]; // the binary string that will contain the 1's and 0's
    int i;
    for (i = 63; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[64]='\0';
    printf("%s",outstr);
}

void nullHandshake(struct handshake *hs)
{
    hs->host = NULL;
    hs->origin = NULL;
    hs->resource = NULL;
    hs->key = NULL;
    hs->frameType = WS_EMPTY_FRAME;
}

static char* getUptoLinefeed(const char *startFrom)
{
    char *write_to = NULL;
    uint8_t new_length = strstr_P(startFrom, rn) - startFrom;
    assert(new_length);
    write_to = (char *)malloc(new_length+1); //+1 for '\x00'
    assert(write_to);
    memcpy(write_to, startFrom, new_length);
    write_to[ new_length ] = 0;

    return write_to;
}

static void copyUptoLinefeed(const char *startFrom, char *writeTo)
{
    uint8_t newLength = strstr_P(startFrom, rn) - startFrom;
    assert(newLength);
    assert(writeTo);
    memcpy(writeTo, startFrom, newLength);
}

enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
                                  struct handshake *hs)
{
    const char *inputPtr = (const char *)inputFrame;
    const char *endPtr = (const char *)inputFrame + inputLength;

    if (!strstr((const char *)inputFrame, "\r\n\r\n"))
        return WS_INCOMPLETE_FRAME;
	
    if (memcmp_P(inputFrame, PSTR("GET "), 4) != 0)
        return WS_ERROR_FRAME;
    // measure resource size
    char *first = strchr((const char *)inputFrame, ' ');
    if (!first)
        return WS_ERROR_FRAME;
    first++;
    char *second = strchr(first, ' ');
    if (!second)
        return WS_ERROR_FRAME;

    if (hs->resource) {
        free(hs->resource);
        hs->resource = NULL;
    }
    hs->resource = (char *)malloc(second - first + 1); // +1 is for \x00 symbol
    assert(hs->resource);

    if (sscanf_P(inputPtr, PSTR("GET %s HTTP/1.1\r\n"), hs->resource) != 1)
        return WS_ERROR_FRAME;
    inputPtr = strstr_P(inputPtr, rn) + 2;

    /*
        parse next lines
     */
    #define prepare(x) do {if (x) { free(x); x = NULL; }} while(0)
    #define strtolower(x) do { int i; for (i = 0; x[i]; i++) x[i] = tolower(x[i]); } while(0)
    uint8_t connectionFlag = FALSE;
    uint8_t upgradeFlag = FALSE;
    uint8_t subprotocolFlag = FALSE;
    char versionString[2];
    memset(versionString, 0, sizeof(versionString));
    while (inputPtr < endPtr && inputPtr[0] != '\r' && inputPtr[1] != '\n') {
        if (memcmp_P(inputPtr, hostField, strlen_P(hostField)) == 0) {
            inputPtr += strlen_P(hostField);
            prepare(hs->host);
            hs->host = getUptoLinefeed(inputPtr);
        } else
        if (memcmp_P(inputPtr, originField, strlen_P(originField)) == 0) {
            inputPtr += strlen_P(originField);
            prepare(hs->origin);
            hs->origin = getUptoLinefeed(inputPtr);
        } else
        if (memcmp_P(inputPtr, protocolField, strlen_P(protocolField)) == 0) {
            inputPtr += strlen_P(protocolField);
            getUptoLinefeed(inputPtr);
            subprotocolFlag = TRUE;
        } else
        if (memcmp_P(inputPtr, keyField, strlen_P(keyField)) == 0) {
            inputPtr += strlen_P(keyField);
            prepare(hs->key);
            hs->key = getUptoLinefeed(inputPtr);
        } else
        if (memcmp_P(inputPtr, versionField, strlen_P(versionField)) == 0) {
            inputPtr += strlen_P(versionField);
            copyUptoLinefeed(inputPtr, versionString);
        } else
        if (memcmp_P(inputPtr, connectionField, strlen_P(connectionField)) == 0) {
            inputPtr += strlen_P(connectionField);
            char *connectionValue = NULL;
            connectionValue = getUptoLinefeed(inputPtr);
            strtolower(connectionValue);
            assert(connectionValue);
            if (strstr_P(connectionValue, upgrade) != NULL)
                connectionFlag = TRUE;
        } else
        if (memcmp_P(inputPtr, upgradeField, strlen_P(upgradeField)) == 0) {
            inputPtr += strlen_P(upgradeField);
            char *compare = NULL;
            compare = getUptoLinefeed(inputPtr);
            strtolower(compare);
            assert(compare);
            if (memcmp_P(compare, websocket, strlen_P(websocket)) == 0)
                upgradeFlag = TRUE;
        };

        inputPtr = strstr_P(inputPtr, rn) + 2;
    }

    // we have read all data, so check them
    if (!hs->host || !hs->key || !connectionFlag || !upgradeFlag || subprotocolFlag
        || memcmp_P(versionString, version, strlen_P(version)) != 0)
    {
        hs->frameType = WS_ERROR_FRAME;
    }
    
    hs->frameType = WS_OPENING_FRAME;
    return hs->frameType;
}

void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame, 
                          size_t *outLength)
{
    assert(outFrame && *outLength);
    assert(hs->frameType == WS_OPENING_FRAME);
    assert(hs && hs->key);

    char *responseKey = NULL;
    uint8_t length = strlen(hs->key)+strlen_P(secret);
    responseKey = malloc(length);
    memcpy(responseKey, hs->key, strlen(hs->key));
    memcpy_P(&(responseKey[strlen(hs->key)]), secret, strlen_P(secret));
    char shaHash[20];
    memset(shaHash, 0, sizeof(shaHash));
    sha1(shaHash, responseKey, length*8);
    base64enc(responseKey, shaHash, 20);

    int written = sprintf_P((char *)outFrame,
                            PSTR("HTTP/1.1 101 Switching Protocols\r\n"
                                 "%s%s\r\n"
                                 "%s%s\r\n"
                                 "Sec-WebSocket-Accept: %s\r\n\r\n"),
                            upgradeField,
                            websocket,
                            connectionField,
                            upgrade2,
                            responseKey);
	
    // if assert fail, that means, that we corrupt memory
    assert(written <= *outLength);
    *outLength = written;
}

void wsMakeFrame(const uint8_t *data, size_t dataLength,
                 uint8_t *outFrame, size_t *outLength, enum wsFrameType frameType)
{
    assert(outFrame && *outLength);
    assert(frameType < 0x10);
    if (dataLength > 0)
        assert(data);
	
    outFrame[0] = 0x80 | frameType;
    
    if (dataLength <= 125) {
        outFrame[1] = dataLength;
        *outLength = 2;
    } else if (dataLength <= 0xFFFF) {
        outFrame[1] = 126;
        uint16_t payloadLength16b = htons(dataLength);
        memcpy(&outFrame[2], &payloadLength16b, 2);
        *outLength = 4;
    } else {
        outFrame[1] = 127;
        memcpy(&outFrame[2], &dataLength, 8);
        *outLength = 10;
    }
    memcpy(&outFrame[*outLength], data, dataLength);
    *outLength+= dataLength;
}

static size_t getPayloadLength(const uint8_t *inputFrame, size_t inputLength,
                               uint8_t *payloadFieldExtraBytes, enum wsFrameType *frameType) 
{
    size_t payloadLength = inputFrame[1] & 0x7F;
    printf("payloadLength %d\n",payloadLength );
    //printf("inputFrams[1] bits: ");
    //show4bits(inputFrame[1]);

    *payloadFieldExtraBytes = 0;

    if ((payloadLength == 0x7E && inputLength < 4)
     || (payloadLength == 0x7F && inputLength < 10)) {
        *frameType = WS_INCOMPLETE_FRAME;
        return 0;
    }
    printf("payloadLength %d\n",payloadLength );
    if (payloadLength == 0x7F && (inputFrame[3] & 0x80) != 0x0) {
        *frameType = WS_ERROR_FRAME;
        return 0;
    }
    printf("payloadLength %d\n",payloadLength );
    printf("payloadLength: ");
    showuint8((uint8_t)payloadLength);

    if (payloadLength == 0x7E) {
        printf("payloadLength == 0x7E \n");
        uint16_t payloadLength16b = 0;
        *payloadFieldExtraBytes = 2;
        memcpy(&payloadLength16b, &inputFrame[2], 4);
        payloadLength = htons(payloadLength16b);
    } else if (payloadLength == 0x7F) {
        printf("payloadLength == 0x7F \n");
        uint64_t payloadLength64b = 0;
        *payloadFieldExtraBytes = 8;
        memcpy(&payloadLength64b, &inputFrame[2], 8);
        if (payloadLength64b > SIZE_MAX) {
            *frameType = WS_ERROR_FRAME;
            return 0;
        }
        payloadLength = (size_t)payloadLength64b;
    }
    printf("payloadLength %d\n",payloadLength );

    return payloadLength;
}

enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength)
{
    printf("inputFrame[0]: ");
    showuint8((uint8_t)inputFrame[0]);

    printf("inputFrame[1]: ");
    showuint8((uint8_t)inputFrame[1]);

    printf("inputFrame[2]: ");
    showuint8((uint8_t)inputFrame[2]);

    printf("inputFrame[3]: ");
    showuint8((uint8_t)inputFrame[3]);

    printf("inputFrame[4]: ");
    showuint8((uint8_t)inputFrame[4]);

    assert(inputFrame && inputLength);

    if (inputLength < 2)
        return WS_INCOMPLETE_FRAME;
    printf("Checks extensions off, bits: ");
	showuint8((uint8_t)(inputFrame[0] & 0x70));
    if ((inputFrame[0] & 0x70) != 0x0) // checks extensions off
        return WS_ERROR_FRAME;
    if ((inputFrame[0] & 0x80) != 0x80) // we haven't continuation frames support
        return WS_ERROR_FRAME; // so, fin flag must be set
    if ((inputFrame[1] & 0x80) != 0x80) // checks masking bit
        return WS_ERROR_FRAME;

    uint8_t opcode = inputFrame[0] & 0x0F;
    if (opcode == WS_TEXT_FRAME ||
            opcode == WS_BINARY_FRAME ||
            opcode == WS_CLOSING_FRAME ||
            opcode == WS_PING_FRAME ||
            opcode == WS_PONG_FRAME
    ) 
    { 
        enum wsFrameType frameType = opcode;

        uint8_t payloadFieldExtraBytes = 0;
        size_t payloadLength = 0;
        printf("%d = %d\n",payloadLength,(inputLength-6-payloadFieldExtraBytes));
        payloadLength = getPayloadLength(inputFrame, inputLength,
                                                &payloadFieldExtraBytes, &frameType);
        printf("%d = %d\n",payloadLength,(inputLength-6-payloadFieldExtraBytes));
        if (payloadLength > 0) {
            if (payloadLength < inputLength-6-payloadFieldExtraBytes) // 4-maskingKey, 2-header
                return WS_INCOMPLETE_FRAME;
            uint8_t *maskingKey = &inputFrame[2 + payloadFieldExtraBytes];

            printf("%d = %d\n",payloadLength,(inputLength-6-payloadFieldExtraBytes));
            assert(payloadLength == inputLength-6-payloadFieldExtraBytes);

            *dataPtr = &inputFrame[2 + payloadFieldExtraBytes + 4];
            *dataLength = payloadLength;
		
            size_t i;
            for (i = 0; i < *dataLength; i++) {
                (*dataPtr)[i] = (*dataPtr)[i] ^ maskingKey[i%4];
            }
        }
        return frameType;
    }

    return WS_ERROR_FRAME;
}
