
#include <stdlib.h>
#include <string.h>

/*from NATTools*/
#include <sockaddr_util.h>

#include "jsmn.h"
#include "json.h"


const char* start_brace_node = "{";
const char* stop_brace_node  = "}";

static int
jsoneq(const char* json,
       jsmntok_t*  tok,
       const char* s)
{
  if ( (tok->type == JSMN_STRING) &&
       ( (int) strlen(s) == tok->end - tok->start ) &&
       (strncmp(json + tok->start, s, tok->end - tok->start) == 0) )
  {
    return 0;
  }
  return -1;
}

int
parseCandidateJson(ICELIB_INSTANCE* icelib,
                   char*            buffer,
                   size_t           len)
{

  int  mediaidx = -1;
  bool foundCline, foundUfrag, foundPasswd = false;
  char cand_addr_str[SOCKADDR_MAX_STRLEN];
  char def_addr_str[SOCKADDR_MAX_STRLEN];
  char ufrag[ICELIB_UFRAG_LENGTH];
  char passwd[ICE_MAX_PASSWD_LENGTH];

  int r;
  (void)icelib;
  jsmn_parser p;
  jsmntok_t   t[2048];     /* We expect no more than 128 tokens */

  jsmn_init(&p);
  r = jsmn_parse( &p, buffer, len + 1, t, sizeof(t) / sizeof(t[0]) );
  if (r < 0)
  {
    printf("Failed to parse JSON: %d\n", r);
    return 1;
  }
  /* Assume the top-level element is an object */
  if ( (r < 1) || (t[0].type != JSMN_OBJECT) )
  {
    printf("Object expected\n");
    return 1;
  }

  /* Loop over all keys of the root object */
  for (int i = 1; i < r; i++)
  {
    if (jsoneq(buffer, &t[i], "default-candidate") == 0)
    {
      foundCline = true;
      if (SOCKADDR_MAX_STRLEN > t[i + 1].end - t[i + 1].start)
      {
        strncpy(def_addr_str,
                buffer + t[i + 1].start,
                t[i + 1].end - t[i + 1].start);
        strcpy(def_addr_str + t[i + 1].end - t[i + 1].start, "\0");
      }
      else
      {
        strncpy(def_addr_str, "0.0.0.0", SOCKADDR_MAX_STRLEN);
      }
      i++;
    }
    else if (jsoneq(buffer, &t[i], "ice-ufrag") == 0)
    {
      foundUfrag = true;
      if (ICELIB_UFRAG_LENGTH > t[i + 1].end - t[i + 1].start)
      {
        strncpy(ufrag, buffer + t[i + 1].start, t[i + 1].end - t[i + 1].start);
      }
      else
      {
        strncpy(ufrag, "void", ICELIB_UFRAG_LENGTH);
      }
      i++;
    }
    else if (jsoneq(buffer, &t[i], "ice-passwd") == 0)
    {
      foundPasswd = true;
      if (ICELIB_PASSWD_LENGTH > t[i + 1].end - t[i + 1].start)
      {
        strncpy(passwd, buffer + t[i + 1].start, t[i + 1].end - t[i + 1].start);
      }
      else
      {
        strncpy(passwd, "void", ICELIB_PASSWD_LENGTH);
      }
      i++;
    }
    else if (jsoneq(buffer, &t[i], "candidates") == 0)
    {
      if (t[i + 1].type != JSMN_ARRAY)
      {
        bool          fFnd, fComp, fTrnsp, fPri, fAddr, fPort, fTyp;
        ICE_CANDIDATE ice_cand;
        int           j;
        for (j = 0; j < (r - i); j++)
        {
          if (jsoneq(buffer, &t[i + j], "foundation") == 0)
          {
            if (ICELIB_FOUNDATION_LENGTH >
                t[i + j + 1].end - t[i + j + 1].start)
            {
              strncpy(ice_cand.foundation, buffer + t[i + j + 1].start,
                      t[i + j + 1].end - t[i + j + 1].start);
            }
            else
            {
              strncpy(ice_cand.foundation, "UK", 2);
            }
            fFnd = true;
            j++;
          }
          else if (jsoneq(buffer, &t[i + j], "component-id") == 0)
          {
            char tmpCmp[6];
            if (6 > t[i + j + 1].end - t[i + j + 1].start)
            {
              strncpy(tmpCmp, buffer + t[i + j + 1].start,
                      t[i + j + 1].end - t[i + j + 1].start);
              ice_cand.componentid = atoi(tmpCmp);
            }
            else
            {
              ice_cand.componentid = 0;
            }
            fComp = true;
            j++;
          }
          else if (jsoneq(buffer, &t[i + j], "transport") == 0)
          {
            if (strncmp(buffer + t[i + j + 1].start, "UDP",3) == 0)
            {
              ice_cand.transport = ICE_TRANS_UDP;
            }
            if (strncmp(buffer + t[i + j + 1].start, "TCP",3) == 0)
            {
              ice_cand.transport = ICE_TRANS_TCPACT;
            }
            fTrnsp = true;
            j++;
          }
          else if (jsoneq(buffer, &t[i + j], "priority") == 0)
          {
            char tmpPri[11];
            if (11 > t[i + j + 1].end - t[i + j + 1].start)
            {
              strncpy(tmpPri, buffer + t[i + j + 1].start,
                      t[i + j + 1].end - t[i + j + 1].start);
              ice_cand.priority = atoi(tmpPri);
            }
            else
            {
              ice_cand.priority = 0;
            }
            fPri = true;
            j++;
          }
          else if (jsoneq(buffer, &t[i + j], "connection-address") == 0)
          {
            if (SOCKADDR_MAX_STRLEN > t[i + j + 1].end - t[i + j + 1].start)
            {
              strncpy(cand_addr_str,
                      buffer + t[i + j + 1].start,
                      t[i + j + 1].end - t[i + j + 1].start);
              strcpy(cand_addr_str + t[i + j + 1].end - t[i + j + 1].start,
                     "\0");
              sockaddr_initFromString(
                (struct sockaddr*)&ice_cand.connectionAddr,
                cand_addr_str);
            }
            else
            {
              strncpy(cand_addr_str, "0.0.0.0", SOCKADDR_MAX_STRLEN);
              sockaddr_initFromString(
                (struct sockaddr*)&ice_cand.connectionAddr,
                cand_addr_str);
            }
            fAddr = true;
            j++;
          }
          else if (jsoneq(buffer, &t[i + j], "port") == 0)
          {
            char tmpPort[6];
            if (6 > t[i + j + 1].end - t[i + j + 1].start)
            {
              strncpy(tmpPort,
                      buffer + t[i + j + 1].start,
                      t[i + j + 1].end - t[i + j + 1].start);
              sockaddr_setPort( (struct sockaddr*)&ice_cand.connectionAddr,
                                atoi(tmpPort) );
            }
            else
            {
              sockaddr_setPort( (struct sockaddr*)&ice_cand.connectionAddr,
                                0 );
            }
            fPort = true;
            j++;
          }
          else if (jsoneq(buffer, &t[i + j], "cand-type") == 0)
          {
            if (strcmp(buffer + t[i + j + 1].start, "host") == 0)
            {
              ice_cand.type = ICE_CAND_TYPE_HOST;

            }
            fTyp = true;
            j++;
          }
          if (fFnd && fComp && fTrnsp && fPri && fAddr && fPort && fTyp)
          {
            int32_t res = ICELIB_addRemoteCandidate(icelib,
                                                    mediaidx,
                                                    ice_cand.foundation,
                                                    strlen(ice_cand.foundation),
                                                    ice_cand.componentid,
                                                    ice_cand.priority,
                                                    cand_addr_str,
                                                    sockaddr_ipPort( (struct
                                                                      sockaddr*)
                                                                     &
                                                                     ice_cand.
                                                                     connectionAddr ),
                                                    ice_cand.transport,
                                                    ice_cand.type);
            if (res == -1)
            {
              printf("Failed to add Remote candidates. (%s)\n",
                     ice_cand.foundation);
            }
            fFnd   = false;
            fComp  = false;
            fTrnsp = false;
            fPri   = false;
            fAddr  = false;
            fPort  = false;
            fTyp   = false;
          }
        }
        i += j;
      }
    }
    else
    {
      printf("Unexpected key: %.*s\n", t[i].end - t[i].start,
             buffer + t[i].start);
    }
    if ( foundCline && foundUfrag && foundPasswd && (mediaidx == -1) )
    {
      struct sockaddr_storage ss;
      sockaddr_initFromString( (struct sockaddr*)&ss,
                               def_addr_str );
      mediaidx = ICELIB_addRemoteMediaStream(icelib,
                                             ufrag,
                                             passwd,
                                             (struct sockaddr*)&ss);
    }

  }
  return EXIT_SUCCESS;
}



size_t
catKeyValue(char*       str,
            const char* key,
            const char* val,
            size_t      len,
            bool        end)
{
  size_t addlen = strlen(key) + strlen(val) + strlen(delim) + 2 + 1;
  if (addlen > len)
  {
    return 0;
  }
  strncat( str, key,   strlen(key) );
  strncat( str, delim, strlen(delim) );
  strncat( str, "\"",  1);
  strncat( str, val,   strlen(val) );
  strncat( str, "\"",  1);
  if (!end)
  {
    strncat(str, ",", 1);
    addlen--;
  }
  return addlen;
}

int
candidatetoJson(char*                string,
                const ICE_CANDIDATE* cand,
                size_t               len)
{
  char   addr_str[SOCKADDR_MAX_STRLEN];
  char   int_str[20];
  size_t left = len;
  strncat(string, start_brace_node, 1);
  left--;
  /* Adding foundation */
  left -= catKeyValue(string, foundation_key, cand->foundation, left, false);
  /* Adding component-id */
  sprintf(int_str, "%d", cand->componentid);
  left -= catKeyValue(string,
                      component_id_key,
                      int_str,
                      left,
                      false);
  /* Adding transport */
  left -= catKeyValue(string,
                      transport_key,
                      ICELIBTYPES_ICE_TRANSPORT_PROTO_toString(cand->transport),
                      left,
                      false);
  /* Adding priority */
  sprintf(int_str, "%d", cand->priority);
  left -= catKeyValue(string, priority_key, int_str, left, false);
  /* Adding connection-address */
  sockaddr_toString( (struct sockaddr*)&cand->connectionAddr,
                     addr_str,
                     sizeof(addr_str),
                     false );

  left -= catKeyValue(string,
                      connection_address_key,
                      addr_str,
                      left,
                      false);
  /* Adding port */

  sprintf( int_str, "%d",
           sockaddr_ipPort( (struct sockaddr*)&cand->connectionAddr ) );
  left -= catKeyValue(string, port_key, int_str, left, false);
  /* Adding cand-type */
  left -= catKeyValue(string,
                      cand_type_key,
                      ICELIBTYPES_ICE_CANDIDATE_TYPE_toString(cand->type),
                      left,
                      true);
  strncat(string, stop_brace_node, 1);
  left--;
  return len - left;

}


int
crateCandidateJson(ICELIB_INSTANCE* icelib,
                   char**           json)
{


  int    mediaidx = 0;
  size_t left     = 0;

  const ICE_MEDIA_STREAM* media =
    ICELIB_getLocalMediaStream(icelib,
                               mediaidx);
  size_t jsonSize = 9000;
  *json = calloc( jsonSize, sizeof(char) );

  left = jsonSize;
  char addr_str[SOCKADDR_MAX_STRLEN];

  strncpy(*json, start_brace_node, 2);
  left = left - strlen(start_brace_node);
  /* Default candidate */
  sockaddr_toString( (struct sockaddr*)&media->candidate[0].connectionAddr,
                     addr_str,
                     sizeof(addr_str),
                     false );
  left -= catKeyValue(*json, default_candidate_key, addr_str, left, false);
  left -= catKeyValue(*json, ice_ufrag_key, media->ufrag, left, false);
  left -= catKeyValue(*json, ice_passwd_key, media->passwd, left, false);
  strncat(*json, "\"candidates\" : { \"candidate\": [ ", left);

  left -= strlen(*json);

  size_t numCandidates = media->numberOfCandidates;
  for (size_t i = 0; i < media->numberOfCandidates; i++)
  {

    left -= candidatetoJson(*json,
                            &media->candidate[i],
                            left);

    /* strncat(*json,"\n", left); */
    /* left--; */
    if (i != numCandidates - 1)
    {
      strncat(*json,",", left);
      left--;
    }
  }
  strncat(*json,"]", left);
  strcat(*json, "}");
  strcat(*json, "}");
  return 0;
}
