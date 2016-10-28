#pragma once

#include <icelib.h>
#include "candidate_harvester.h"


static const char delim[]                  = " : ";
static const char default_candidate_key[]  = "\"default-candidate\"";
static const char ice_ufrag_key[]          = "\"ice-ufrag\"";
static const char ice_passwd_key[]         = "\"ice-passwd\"";
static const char foundation_key[]         = "\"foundation\"";
static const char component_id_key[]       = "\"component-id\"";
static const char transport_key[]          = "\"transport\"";
static const char priority_key[]           = "\"priority\"";
static const char connection_address_key[] = "\"connection-address\"";
static const char port_key[]               = "\"port\"";
static const char cand_type_key[]          = "\"cand-type\"";
static const char to_key[]                 = "\"to_ip\"";

int
crateCandidateJson(ICELIB_INSTANCE* icelib,
                   char**           json);

int
parseCandidateJson(ICELIB_INSTANCE* icelib,
                   char*            buffer,
                   size_t           len);
