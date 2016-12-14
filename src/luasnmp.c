
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#define DISABLE_MIB_LOADING 1


//#include <net-snmp/library/oid.h>
#include <stdio.h>
#include <stdbool.h>


typedef u_long myoid;

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

char *oid_to_sring (myoid anOID[], int len)
{
    char buffer[1024];
    
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, NETSNMP_OID_OUTPUT_NUMERIC);
    memset (buffer, 0, sizeof (buffer));
    if(snprint_objid(buffer, sizeof(buffer)-1, anOID, len) == -1) return NULL;
    return strdup (buffer);
}

char *var_to_sring (const oid *objid, size_t objidlen, const netsnmp_variable_list *variable)
{
    char buffer[1024];
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT, true);
    memset (buffer, 0, sizeof (buffer));
    if(snprint_value(buffer, sizeof(buffer)-1, objid, objidlen, variable) == -1) return NULL;
    return strdup (buffer);
}



char* snmp_get_v12 (const char* host, const char *oid, const char* community, int version) {
    struct snmp_session session, *ss;
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    char *result = NULL;
    myoid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    
    struct variable_list *vars;
    int status;

    snmp_sess_init (&session);
    session.peername = (char *)host;
    session.version = version = version;
    session.community = (unsigned char *)community;
    session.community_len = strlen (community);
   
    ss = snmp_open (&session);
    if (!ss) return NULL;
    
    pdu = snmp_pdu_create (SNMP_MSG_GET);
    read_objid (oid, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
   
    status = snmp_synch_response (ss, pdu, &response);
   
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        vars = response->variables;
        result = var_to_sring (vars->name, vars->name_length, vars);
    }
    if (response) snmp_free_pdu (response);
    snmp_close(ss);
    return result;
}

bool snmp_getnext_v12 (const char* host, const char *oid, const char* community, int version, char **resultoid, char **resultvalue) {
    struct snmp_session session, *ss;
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    char *result = NULL;
    unsigned long int anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char buffer[1024];
    
    struct variable_list *vars;
    int status;

    snmp_sess_init (&session);
    session.peername = (char *)host;
    session.version = version = version;
    session.community = (unsigned char *)community;
    session.community_len = strlen (community);
   
    ss = snmp_open (&session);
    if (!ss) return false;
    
    pdu = snmp_pdu_create (SNMP_MSG_GETNEXT);
    read_objid (oid, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
   
    status = snmp_synch_response (ss, pdu, &response);
   
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        
        vars = response->variables; // we should have just one variable
        snprint_objid(buffer, 1024, vars->name, vars->name_length);
        
        switch (vars->type) {
        case ASN_OCTET_STR:
            result = malloc(1 + vars->val_len);
            memcpy(result, vars->val.string, vars->val_len);
            result[vars->val_len] = '\0';
            break;
        case ASN_INTEGER:
        case ASN_GAUGE:
        case ASN_COUNTER:
        case ASN_TIMETICKS:
            if (asprintf (&result, "%ld", *vars->val.integer) == -1) result = false;
            break;
        default:
            result = false;
            break;
        }
    }
       
    if (response) snmp_free_pdu (response);
    snmp_close(ss);
    return result;
}


void luasnmp_init() {
    init_snmp("fty-snmp-client");
}

static int snmp_get(lua_State *L){
	char* host = lua_tostring(L, 1);
	char* oid = lua_tostring(L, 2);
    //printf ("lua is calling %s %s\n", host, oid);
    //TODO: get credentials/snmpversion for host
    char *result = snmp_get_v12 (host, oid, "public", SNMP_VERSION_1);
    if (result) {
        lua_pushstring (L, result);
        free (result);
        return 1;
    } else {
        return 0;
    }
}

void extend_lua_of_snmp(lua_State *L) {
    lua_register (L, "snmp_get", snmp_get);
    //TODO: lua_register (L, "snmp_getnext", snmp_getnext);
}

int main() {
    luasnmp_init ();
    char *value = snmp_get_v12 ("10.130.53.36", ".1.3.6.1.2.1.1.1.0", "public", SNMP_VERSION_1);
    printf("value: %s\n", value ? value : "null");
    free (value);

    // lua snmp test
    lua_State *l = lua_open();
    luaL_openlibs(l); // get functions like print();
    extend_lua_of_snmp (l); //extend of snmp
    luaL_dostring (l, "function main(host) print (snmp_get (host,'.1.3.6.1.2.1.1.1.0')) end");
    lua_settop (l, 0);
    lua_getglobal (l, "main");
    lua_pushstring (l, "10.130.53.36");
    lua_pcall (l, 1, 1, 0);
    lua_close (l);
    return 0;
}
