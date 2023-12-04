# Sequence Diagram Iso

```mermaid
sequenceDiagram

    box rgb(99,33,66) iso.c
    participant chan_send as add header
    end

    box rgb(33,99,66) conn.c
    participant conn_iso_send as bt_conn_send_iso_cb()
    participant conn_send as bt_conn_send_cb()
    participant queue2 as tx_queue
    participant process as bt_conn_process_tx()
    participant send_buf as send_buf()
    participant send_frag as send_frag()
    participant do_send_frag as do_send_frag()
    participant send_iso as send_iso()
    end

    box rgb(33,66,99) hci_core.c
    participant bt_send as bt_send()
    end
    
    chan_send->>conn_iso_send: iso packet
    conn_iso_send->>conn_send: iso packet
    conn_send->>+queue2: iso packet
    queue2->>-process: iso_packet
    process->>send_buf: iso_packet
    send_buf->>send_frag: iso_packet
    send_frag->>do_send_frag: iso_packet
    do_send_frag->>send_iso: iso_packet
    send_iso->>bt_send: iso_packet