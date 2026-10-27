/* Build-pinned lookup for an AI component absent from the SDK's component catalog.
 * The full ownership map and native getter must match before using this layout. */
#include "movement_lookup_data.h"
static const char *m_lookup_reason;
static uintptr_t m_lookup_fail(const char *reason){m_lookup_reason=reason;return 0;}
static uintptr_t m_resolve(void)
{
    size_t image_size=0;
    uintptr_t base=m_api->module_base("game.dll",&image_size);
    if(base<65536 || image_size!=74727424u || base>UINT64_C(0x800000000000)-image_size)
        return m_lookup_fail("game module layout changed");
    uint8_t code[sizeof m_native_getter],header[sizeof m_table_header],map[sizeof m_owner_map];
    uintptr_t root=0,table=0,root_again=0,table_again=0;
    if(m_api->read(base+0x511CC0u,code,sizeof code) || memcmp(code,m_native_getter,sizeof code))
        return m_lookup_fail("native AI lookup signature changed");
    if(m_api->read(base+0x346BF98u,&root,8) || root<65536 ||
       root>UINT64_C(0x800000000000)-0xF12DF0u || !m_region(root+0xF12DE8u,8))
        return m_lookup_fail("entity root unavailable or unsafe");
    if(m_api->read(root+0xF12DE8u,&table,8) || table<65560 || !m_region(table-24,120736u))
        return m_lookup_fail("AI component table unavailable or unsafe");
    if(m_api->read(table-24,header,sizeof header) || memcmp(header,m_table_header,sizeof header))
        return m_lookup_fail("AI component header or table size changed");
    if(m_api->read(table,map,sizeof map) || memcmp(map,m_owner_map,sizeof map))
        return m_lookup_fail("AI resource ownership map changed");
    /* This exact pinned map owns row70 once, exclusively under Alpha Warrior's
     * resource B2A6FA1E4284C7E6. Check that the root, table and map stayed current. */
    if(m_api->read(base+0x346BF98u,&root_again,8) || root_again!=root ||
       m_api->read(root+0xF12DE8u,&table_again,8) || table_again!=table ||
       m_api->read(table-24,header,sizeof header) || memcmp(header,m_table_header,sizeof header) ||
       m_api->read(table,map,sizeof map) || memcmp(map,m_owner_map,sizeof map))
        return m_lookup_fail("AI table changed during lookup");
    m_lookup_reason=NULL;
    return table+sizeof m_owner_map+70u*M_SIZE;
}
