//#include "GamePacketHeader.h"
//#include <iostream>
//
//// Serialize packet data into a byte buffer
//void GamePacketHeader::serialize(char* buffer) const {
//    std::memcpy(buffer, &packetID, sizeof(packetID));
//    std::memcpy(buffer + sizeof(packetID), &timestamp, sizeof(timestamp));
//    std::memcpy(buffer + sizeof(packetID) + sizeof(timestamp), &packetType, sizeof(packetType));
//    std::memcpy(buffer + sizeof(packetID) + sizeof(timestamp) + sizeof(packetType), &posX, sizeof(posX));
//    std::memcpy(buffer + sizeof(packetID) + sizeof(timestamp) + sizeof(packetType) + sizeof(posX), &posY, sizeof(posY));
//    std::memcpy(buffer + sizeof(packetID) + sizeof(timestamp) + sizeof(packetType) + sizeof(posX) + sizeof(posY), &posZ, sizeof(posZ));
//}
//
//// Deserialize packet data from a byte buffer
//void GamePacket::deserialize(const char* buffer) {
//    std::memcpy(&packetID, buffer, sizeof(packetID));
//    std::memcpy(&timestamp, buffer + sizeof(packetID), sizeof(timestamp));
//    std::memcpy(&packetType, buffer + sizeof(packetID) + sizeof(timestamp), sizeof(packetType));
//    std::memcpy(&posX, buffer + sizeof(packetID) + sizeof(timestamp) + sizeof(packetType), sizeof(posX));
//    std::memcpy(&posY, buffer + sizeof(packetID) + sizeof(timestamp) + sizeof(packetType) + sizeof(posX), sizeof(posY));
//    std::memcpy(&posZ, buffer + sizeof(packetID) + sizeof(timestamp) + sizeof(packetType) + sizeof(posX) + sizeof(posY), sizeof(posZ));
//}
//
//// Get recipient address
//sockaddr_in GamePacket::getRecipient() const {
//    return recipientAddr;
//}
//
//// Set recipient address
//void GamePacket::setRecipient(const sockaddr_in& addr) {
//    recipientAddr = addr;
//}
//
//// Convert packet data to string for debugging
//std::string GamePacket::toString() const {
//    return "PacketID: " + std::to_string(packetID) +
//        ", Timestamp: " + std::to_string(timestamp) +
//        ", Type: " + std::to_string(packetType) +
//        ", Position: (" + std::to_string(posX) + ", " +
//        std::to_string(posY) + ", " +
//        std::to_string(posZ) + ")";
//}