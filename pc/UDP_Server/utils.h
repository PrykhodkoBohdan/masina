#pragma once
#include "common.h"
#include "controller.h"
#include "server.h"

std::wstring convert_to_wstring(const std::string& str);
double calculateAngle(double lat1, double lon1, double lat2, double lon2);
double calculateHaversine(double lat1, double lon1, double lat2, double lon2);
std::wstring Compass(int angle, double latitude, double longitude, double homeLat, double homeLon, double pinLat, double pinLon);
uint8_t CRC(const std::vector<uint8_t>& data, std::size_t start, std::size_t length);
void printHex(const char* arr, size_t length);
void printVectorHex(const std::vector<uint8_t>& vec, std::size_t maxLength);
int Http_Post(const char* domain, int port, const char* path, char* body, uint16_t bodyLen, char* retBuffer, int bufferLen);
void TraccarUpdate();