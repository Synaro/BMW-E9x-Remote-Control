# Journal de diagnostic embarqué

## Objectif

Le journal conserve les décisions importantes du contrôleur sans enregistrer de
donnée brute provenant du véhicule. Il sert à comprendre un refus, une
transition ou un défaut tout en gardant un coût mémoire fixe et prévisible.

Le composant `DiagnosticJournal` appartient à l'infrastructure. Le contrôleur
reste indépendant de la journalisation et ne connaît ni stockage, ni console,
ni transport USB.

## Contenu d'un enregistrement

Chaque entrée compacte contient uniquement :

- un numéro de séquence local ;
- un temps monotone en millisecondes fourni par le runtime ;
- la catégorie de l'enregistrement ;
- l'événement sémantique déclencheur ;
- l'ancien et le nouvel état du contrôleur ;
- le code de défaut éventuel ;
- le motif sémantique d'un refus ;
- les masques de sécurité et de qualification du profil.

Le journal ne contient volontairement aucun VIN, identifiant CAN, octet de
trame, valeur de capteur, position, identifiant de clé ou donnée personnelle.

## Catégories

- `command_received` : demande de démarrage, d'arrêt, de reprise ou de reset ;
- `state_transition` : changement effectif de la machine d'état ;
- `request_rejected` : demande désactivée, profil non prêt, sécurité refusée ou
  commande incompatible avec l'état courant ;
- `fault_entered` : entrée dans l'état `Fault` ;
- `infrastructure_failure` : échec du véhicule, d'un actionneur ou d'un timer ;
- `safing_failure` : échec supplémentaire pendant la tentative de mise en
  sécurité.

Une mise à jour véhicule qui ne produit ni transition, ni refus, ni défaut
n'est pas enregistrée. Cette règle empêche les messages périodiques de saturer
le journal.

## Bornes mémoire

La capacité est fixée à 32 enregistrements. Lorsque le tableau est plein, le
plus ancien est remplacé et `overwrittenCount()` est incrémenté. La lecture
reste chronologique malgré le rebouclage du tableau.

Chaque enregistrement est limité statiquement à 24 octets au maximum, soit au
plus 768 octets pour les données du journal. Le composant n'utilise aucune
allocation dynamique et son compteur de pertes sature au lieu de déborder.

Le contenu est actuellement volatil : un redémarrage l'efface. Une éventuelle
persistance devra être conçue séparément afin de ne pas augmenter l'usure de la
flash ni retarder la boucle de contrôle.

## Consultation dans le simulateur

Tous les scénarios qui exécutent le runtime affichent le journal avant leur
résultat :

```powershell
.\scripts\simulate.ps1 -Scenario nominal
.\scripts\simulate.ps1 -Scenario hood-required
```

Exemple de ligne :

```text
#7 time_ms=5000 type=fault_entered trigger=vehicle_state_updated state=running->fault fault=safety_interlock reason=safety_policy safety_mask=4 profile_mask=0
```

L'accès au journal d'un futur boîtier par USB n'est pas encore exposé. Il devra
réutiliser une commande bornée et authentifiée, distincte des réglages.
