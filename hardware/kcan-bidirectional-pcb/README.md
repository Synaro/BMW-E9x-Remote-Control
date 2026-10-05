# Phase 3H — carte K-CAN bidirectionnelle

Ce dossier définit la révision matérielle destinée à remplacer la carte
RX-only Phase 3G/3G.1 pour une future fabrication. Les dossiers et tags des
anciennes phases restent historiques et ne sont pas modifiés.

La carte Phase 3H fournit les deux chemins électriques complets :

```text
GPIO5 / TWAI_TX -> R5 -> U2 A -> U2 B -> U1 TXD -> K-CAN
K-CAN -> U1 RXD -> R7 -> GPIO4 / TWAI_RX
```

U2 est un `SN74LXC1T45QDCKRQ1` alimenté en 3,3 V côté A et 5 V côté B.
Il remplace le buffer mono-alimentation Phase 3G. Il ne constitue pas un
verrou : `DIR` est fixé à A vers B et le chemin TX est toujours câblé.

Il n'existe aucun jumper TX, pont de soudure, composant DNP, `R_LINK_TX`, zone
keepout ou coupure cuivre permettant/désactivant la transmission. Le mode
listen-only reste disponible uniquement comme configuration logicielle TWAI.

## Contenu

- `kicad/` : schéma et PCB KiCad 10 Phase 3H ;
- `BOM_PHASE3H.csv` : BOM normative par carte ;
- `netlist_PHASE3H.csv` : connexions normatives et chemins fonctionnels ;
- `pin-review.csv` : revue des broches critiques ;
- `procurement_PHASE3H.csv` : références de sourcing, sans ordre d'achat ;
- `manufacturing/` : artefacts régénérés depuis cette révision uniquement.

La régénération s'effectue avec :

```powershell
python tools/generate_phase3h_kicad.py schematic
& 'C:\Users\synar\AppData\Local\Programs\KiCad\10.0\bin\python.exe' tools/generate_phase3h_kicad.py pcb
./tools/generate_phase3h_manufacturing.ps1
./tools/generate_phase3h_nextpcb.ps1
```

Ces commandes ne contactent aucun fabricant et ne passent aucune commande.
