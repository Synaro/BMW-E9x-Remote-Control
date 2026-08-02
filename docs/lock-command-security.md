# Sécurité des commandes de verrouillage

## Frontière de confiance

Le noyau ne contient aucun identifiant BMW. `CanLockCommandAdapter` peut convertir
une trame dont la liaison a été qualifiée en `LockCommandEvidence`, puis la
transmettre à `LockCommandGate`. Seule la garde peut alimenter
`LockSequenceDetector` et produire une demande sémantique de démarrage distant.

Une preuve contient uniquement :

- une classe de source (`VehicleAdapter` ou `SyntheticTest`) ;
- son niveau de confiance (`Unverified`, `Candidate` ou `Verified`) ;
- un numéro de séquence de source sur 32 bits ;
- l'instant monotone de l'observation ;
- la confirmation que le véhicule est sécurisé.

Elle ne contient ni VIN, ni identifiant de clé, ni trame véhicule brute. Le mot
`Verified` est une responsabilité de l'adaptateur : il ne constitue pas, à lui
seul, une preuve cryptographique.

## Refus fermés

`LockCommandGate` rejette une preuve dans chacun des cas suivants :

- configuration temporelle impossible ou hors borne ;
- source absente ou niveau différent de `Verified` ;
- source synthétique sans activation explicite réservée aux tests ;
- véhicule non confirmé sécurisé ;
- preuve future ou âgée de plus de 500 ms par défaut ;
- numéro de séquence dupliqué ou antérieur ;
- horodatage de preuve dupliqué ou antérieur ;
- recul de l'horloge applicative ;
- impulsion située dans la fenêtre anti-rebond.

Les compteurs et horloges acceptent le rebouclage naturel sur 32 bits. Une
distance inférieure à la moitié de leur plage est considérée comme postérieure,
ce qui évite de confondre un rebouclage légitime avec un recul.

Un refus structurel efface toute séquence de verrouillage partiellement reconnue.
Une impulsion anti-rebond n'est pas comptée, mais conserve les appuis légitimes
déjà observés. Après un redémarrage du boîtier, la garde repart sans historique ;
la qualification de l'adaptateur devra donc aussi définir le comportement de la
source au démarrage.

## Ce que la garde prouve — et ne prouve pas

La garde protège la frontière applicative contre les objets dupliqués, anciens,
désordonnés ou injectés depuis une source non autorisée. Elle empêche également
le simulateur d'être accepté par une configuration de production.

Elle ne prouve pas encore qu'une trame observée sur le véhicule provient de la
clé d'origine. Elle ne bloque pas à elle seule le rejeu d'une trame BMW brute.
`CanLockCommandAdapter` exige donc un compteur roulant qualifié et rejette les
répétitions, mais cette protection ne vaut que si le champ retenu est réellement
lié à la provenance attendue. La future qualification BMW devra déterminer si
ce compteur, une corrélation CAS/JBE, une confirmation d'état ou une autre
propriété légitime permet de construire un numéro de séquence fiable. Jusqu'à
cette qualification, aucun adaptateur réel ne doit marquer une commande
`Verified`.

Le scénario `lock-replay-guard` utilise volontairement `SyntheticTest` avec une
autorisation explicite. Il démontre les règles du noyau, pas la sécurité du bus
BMW réel.
