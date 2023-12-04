# Sequence Diagram Iso

```mermaid
sequenceDiagram


    box rgb(33,99,66) conn.c
    participant process as send_iso()
    end

    box rgb(33,66,99) hci_core.c
    participant bt_send as bt_send()
    end

    box rgb(99,66,33) hci_driver.c
    participant driver_send as hci_send()
    end

    box rgb(99,33,66) hci.c
    participant hci_handle as hci_iso_handle()
    end



    process->>bt_send: iso_packet
    bt_send->>driver_send: iso_packet
    driver_send->>hci_handle: iso_packet