# Sequence Diagram Iso

```mermaid
sequenceDiagram
    box rgb(33,66,99) hci
    participant hci_handle as hci_iso_handle()
    end

    box rgb(99,66,33) isoal.c
    participant fragment as isoal_tx_sdu_fragment()
    participant produce as isoal_tx_unframed_produce()
    participant try_emit as isoal_tx_try_emit_pdu()
    participant emit as isoal_tx_pdu_emit()
    end

    box rgb(99,33,66) ull_iso.c
    participant queue3 as ll_iso_tx_mem_enqueue()
    end
    
    hci_handle->>fragment: iso_packet
    fragment->>produce: iso_packet
    produce->>try_emit: iso_packet
    try_emit->>emit: iso_packet
    emit->>+queue3: iso_packet