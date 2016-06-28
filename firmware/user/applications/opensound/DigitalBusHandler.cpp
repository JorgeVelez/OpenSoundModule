#include "DigitalBusHandler.h"
#include "MidiStatus.h"
#include "application.h"
#include "opensound.h"

DigitalBusHandler::DigitalBusHandler() 
  : uid(NO_UID), nuid(NO_UID), token(NO_TOKEN), peers(0), parameterOffset(0) {
  // UUID = (uint8_t*) = Particle.deviceID()
  Particle.deviceID().getBytes(UUID, sizeof(UUID)); // returns string pointer
}

// send a 4-byte message
void DigitalBusHandler::sendMessage(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4){
  uint8_t buf[4] = {d1, d2, d3, d4};
  sendFrame(buf);
}

void DigitalBusHandler::sendFrame(uint8_t* frame){
  Serial1.write(frame, 4);
  // serial_write(frame, 4);
}

uint32_t DigitalBusHandler::generateToken(){
  uint32_t tok = (VERSION << 16) | UUID[11];
  tok ^= (UUID[10] << 16) | (UUID[9] << 8) | UUID[8];
  tok ^= (UUID[7] << 16) | (UUID[6] << 8) | UUID[5];
  tok ^= (UUID[4] << 16) | (UUID[3] << 8) | UUID[2];
  tok ^= (UUID[1] << 16) | (UUID[0] << 8) | PRODUCT;
  return tok;
}

void DigitalBusHandler::startDiscover(){
  // while(token == NO_TOKEN) // todo: what if generated token == NO_TOKEN?
  debug << "startDiscover [" << uid << "][" << token << "]\r\n";
  if(token == NO_TOKEN)
    token = generateToken();
  if(peers == 0){
    uid = 0; // start by assuming we will be UID 0
    sendDiscover(0, token);
  }
}

void DigitalBusHandler::sendDiscover(uint8_t seq, uint32_t token){
  sendMessage(OWL_COMMAND_DISCOVER|seq, token>>16, token>>8, token);
  debug << "Sent discovery token [" << token << "]\r\n";
}

void DigitalBusHandler::handleDiscover(uint8_t seq, uint32_t other){
  debug << "rx disco [" << seq << "][" << other << "]\r\n";
  // on receipt of other token, add +1 to seq and pass it on, but send own token first.
  // once we get our own token back, the seq tells us how many peers there are.
  // lowest token then takes precedence.
  if(token == NO_TOKEN){
    startDiscover();
  }else if(other == token){
    // that's our token.
    peers = seq;
    if(uid == 0)
      // note that this will push out more messages before 
      // the DISCOVERs have finished coming through
      startEnum();
    return;
  }else if(other > token){
    uid = NO_UID; // we will not be UID 0
  }
  if(seq < 0x0f)
    sendDiscover(seq+1, other);
}

void DigitalBusHandler::startEnum(){
  parameterOffset = 0;
  sendEnum(uid, VERSION, PRODUCT, parameterOffset+PARAMETERS);
  debug << "Sent enum uid [" << uid << "]\r\n";
}

void DigitalBusHandler::sendEnum(uint8_t id, uint8_t version, uint8_t product, uint8_t params){
  sendMessage(OWL_COMMAND_ENUM|id, version, product, params);
}

void DigitalBusHandler::handleEnum(uint8_t id, uint8_t version, uint8_t product, uint8_t params){
  debug << "rx enum [" << id << "][" << version << "][" << product << "][" << params << "]\r\n";
  if(uid == NO_UID){
    // our UID has not been set yet
    // set it and pass on incremented value
    uid = id+1;
    parameterOffset = params;
    sendEnum(uid, VERSION, PRODUCT, parameterOffset+PARAMETERS);
    // note that only one ENUM should be received as they are not propagated.
    // downstream UID will be (uid+1 > peers) ? 0 : uid+1
    nuid = uid+1;
    if(nuid >= peers)
      nuid = 0;
  }else if(uid == 0){
    nuid = 1;
  }else if(uid == id){
    // we are talking to ourselves: ignore
  }else{
    // something must have gone wrong, we already have a UID
    // but this might be the second round?
    debug << "enum error [" << uid << "] " << "[" << parameterOffset << "]\r\n";
  }
  debug << "enumerated [" << uid << "][" << nuid << "][" << parameterOffset << "]\r\n";
}

void DigitalBusHandler::startIdent(){
  sendIdent(uid, VERSION, PRODUCT, UUID);
  debug << "send ident [" << uid << "]\r\n";
}

void DigitalBusHandler::sendIdent(uint8_t id, uint8_t version, uint8_t device, uint8_t* uuid){
  sendMessage(OWL_COMMAND_IDENT|uid, VERSION, PRODUCT, uuid[15]);
  sendMessage(OWL_COMMAND_IDENT|uid, uuid[14], uuid[13], uuid[12]);
  sendMessage(OWL_COMMAND_IDENT|uid, uuid[11], uuid[10], uuid[9]);
  sendMessage(OWL_COMMAND_IDENT|uid, uuid[8], uuid[7], uuid[6]);
  sendMessage(OWL_COMMAND_IDENT|uid, uuid[5], uuid[4], uuid[3]);
  sendMessage(OWL_COMMAND_IDENT|uid, uuid[2], uuid[1], uuid[0]);
}

/* void handleIdent(uint8_t id, uint8_t version, uint8_t device, uint8_t* uuid){ */
/*   if(id != uid && id != nuid) */
/*     sendIdent(id, version, device, uuid); // pass it on */
/* } */

void DigitalBusHandler::handleIdent(uint8_t id, uint8_t d1, uint8_t d2, uint8_t d3){
  // todo: need to wait for full set of 6 messages and buffer UUID?
  // no because uid is contained in every message
  debug << "rx ident [" << id << "][" << d3 << "]\r\n";
  // propagation done by DigitalBusReader
}

void DigitalBusHandler::sendParameterChange(uint8_t pid, int16_t value){
  debug << "tx param [" << pid << "][" << value << "]\r\n";
  sendMessage(OWL_COMMAND_PARAMETER|uid, pid, value>>8, value);
}

void DigitalBusHandler::handleParameterChange(uint8_t pid, int16_t value){
  debug << "rx param [" << pid << "][" << value << "]\r\n";
  txParameter(pid, value);
  // todo
  // setParameter(pid, value);  
}
