#include <string.h>

#include "uip-split.h"
#include "uip.h"
#include "uip-fw.h"
#include "uip_arch.h"



#define BUF ((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])

/*-----------------------------------------------------------------------------*/
void
uip_split_output(void)
{
  u16_t tcplen, len1, len2;

  /* We only try to split maximum sized TCP segments. */
  if(BUF->proto == UIP_PROTO_TCP &&
     uip_len == UIP_BUFSIZE - UIP_LLH_LEN) {

    tcplen = uip_len - UIP_TCPIP_HLEN;
    /* Split the segment in two. If the original packet length was
       odd, we make the second packet one byte larger. */
    len1 = len2 = tcplen / 2;
    if(len1 + len2 < tcplen) {
      ++len2;
    }

    /* Create the first packet. This is done by altering the length
       field of the IP header and updating the checksums. */
    uip_len = len1 + UIP_TCPIP_HLEN;
#if UIP_CONF_IPV6
    /* For IPv6, the IP length field does not include the IPv6 IP header
       length. */
    BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
    BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);
#else /* UIP_CONF_IPV6 */
    BUF->len[0] = uip_len >> 8;
    BUF->len[1] = uip_len & 0xff;
#endif /* UIP_CONF_IPV6 */
    
    /* Recalculate the TCP checksum. */
    BUF->tcpchksum = 0;
    BUF->tcpchksum = ~(uip_tcpchksum());

#if !UIP_CONF_IPV6
    /* Recalculate the IP checksum. */
    BUF->ipchksum = 0;
    BUF->ipchksum = ~(uip_ipchksum());
#endif /* UIP_CONF_IPV6 */
    
    /* Transmit the first packet. */
    /*    uip_fw_output();*/
    tcpip_output();

    /* Now, create the second packet. To do this, it is not enough to
       just alter the length field, but we must also update the TCP
       sequence number and point the uip_appdata to a new place in
       memory. This place is detemined by the length of the first
       packet (len1). */
    uip_len = len2 + UIP_TCPIP_HLEN;
#if UIP_CONF_IPV6
    /* For IPv6, the IP length field does not include the IPv6 IP header
       length. */
    BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
    BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);
#else /* UIP_CONF_IPV6 */
    BUF->len[0] = uip_len >> 8;
    BUF->len[1] = uip_len & 0xff;
#endif /* UIP_CONF_IPV6 */
    
    /*    uip_appdata += len1;*/
    memcpy(uip_appdata, (u8_t *)uip_appdata + len1, len2);

    uip_add32(BUF->seqno, len1);
    BUF->seqno[0] = uip_acc32[0];
    BUF->seqno[1] = uip_acc32[1];
    BUF->seqno[2] = uip_acc32[2];
    BUF->seqno[3] = uip_acc32[3];
    
    /* Recalculate the TCP checksum. */
    BUF->tcpchksum = 0;
    BUF->tcpchksum = ~(uip_tcpchksum());

#if !UIP_CONF_IPV6
    /* Recalculate the IP checksum. */
    BUF->ipchksum = 0;
    BUF->ipchksum = ~(uip_ipchksum());
#endif /* UIP_CONF_IPV6 */

    /* Transmit the second packet. */
    /*    uip_fw_output();*/
    tcpip_output();
  } else {
    /*    uip_fw_output();*/
    tcpip_output();
  }
     
}
/*-----------------------------------------------------------------------------*/
