# Modèle de sécurité

## Positionnement

Le logiciel adopte une stratégie **fail-safe** : l'absence d'une preuve de
sécurité est traitée comme une condition non sûre. Il ne revendique aucune
certification fonctionnelle ou automobile.

## Invariants du noyau

1. Le démarreur n'est demandé qu'après une autorisation et une préparation
   réussies.
2. Le démarreur est relâché dès que le régime moteur confirme le démarrage.
3. Toute temporisation critique possède une limite.
4. Un défaut d'infrastructure mène à `Fault` et à `SecureOutputs`.
5. Un défaut reste mémorisé jusqu'à un réarmement explicite.
6. Le réarmement exige un régime moteur nul et l'absence d'un défaut véhicule
   critique, tous deux issus de signaux frais.
7. Une boîte manuelle est refusée par défaut.
8. Une donnée périmée ou indisponible ne peut jamais valider une demande.
9. Une deuxième demande de démarrage ne peut pas réengager le démarreur depuis
   un état actif.
10. La durée de fonctionnement distant est bornée.
11. Une demande de démarrage sans profil explicitement sélectionné et qualifié
    reste en `Idle`, sécurise les sorties et n'interroge pas le véhicule.
12. Le capot fermé est exigé par défaut. Une installation sans capteur peut
    explicitement désactiver ce contrôle ; le signal est alors entièrement
    ignoré plutôt que remplacé par une valeur inventée.
13. Une ouverture de portière pendant `Running` ne suffit jamais à conserver le
    moteur en marche : elle ouvre une fenêtre de reprise de 60 secondes.
14. Sans confirmation de reprise authentifiée, le contrôleur ordonne l'arrêt.
15. Après une reprise confirmée, les commandes distantes d'arrêt sont ignorées
    afin de ne pas couper un moteur placé sous contrôle du conducteur.
16. Une configuration utilisateur invalide désactive le démarrage distant au
    lieu d'appliquer une valeur corrigée implicitement.
17. Les durées utilisateur restent dans des bornes codées indépendamment du
    format de stockage ou de l'interface de configuration.
18. Une nouvelle configuration n'est jamais appliquée au milieu d'une session
    distante active.
19. Une configuration persistante n'est acceptée qu'après validation de la
    version, du CRC et de toutes les bornes métier.
20. Si aucun des deux emplacements persistants n'est valide, le démarrage
    distant reste désactivé.
21. Une impulsion de verrouillage n'alimente la séquence de démarrage que si sa
    source est déclarée vérifiée, sa preuve récente et son ordre strictement
    croissant ; tout refus structurel annule la séquence partielle.
22. Le décodeur CAN de verrouillage est désactivé sans liaison qualifiée. Une
    liaison active exige un front montant et un compteur roulant indépendant ;
    toute répétition ou régression efface le geste en cours.
23. Le superviseur d'actionneurs exige un heartbeat récent et une autorisation
    matérielle observée avant toute activation.
24. Le démarreur ne peut être engagé sans allumage commandé et, par défaut,
    confirmé par son retour d'état.
25. Une perte d'autorisation, une expiration, un retour incohérent ou une panne
    du pilote mémorise le défaut et commande immédiatement l'état sûr.
26. Le réarmement du superviseur exige, par défaut, la confirmation électrique
    que l'allumage et le démarreur sont inactifs.

## Matrice d'autorisation initiale

| Condition | Exigence par défaut | Résultat en cas d'échec |
|---|---|---|
| Profil véhicule | Sélectionné, signaux vérifiés, niveau lecture seule | Refus avant communication |
| Capot | Fermé et frais lorsque `requireHoodClosed` vaut `true` | Refus |
| Tension batterie | ≥ 11,8 V et fraîche | Refus |
| Régime moteur | < 500 tr/min et frais | Refus |
| Portes / coffre | Fermés et frais | Refus |
| Frein de service | Relâché et frais | Refus |
| Frein de stationnement | Serré et frais | Refus |
| Défaut véhicule critique | Absent et frais | Refus |
| Boîte automatique | Position `Park` fraîche | Refus |
| Boîte manuelle | Interdite par défaut | Refus |

La politique peut autoriser explicitement une boîte manuelle, auquel cas un
signal `Neutral` frais et le frein de stationnement restent nécessaires. Cette
option logicielle ne remplace pas un interverrouillage matériel indépendant et
ne doit pas être activée sans conception de sécurité dédiée.

## Surveillance pendant la session

Pendant `Preparing`, `Cranking` et `Running`, les mises à jour véhicule sont
réévaluées. Le coffre, le capot lorsqu'il est requis, un changement de rapport,
un frein actionné, une perte de fraîcheur ou un défaut critique mène à `Fault`
avec sécurisation des sorties.

En `Running`, l'ouverture d'une portière constitue l'unique exception : elle
passe à `AwaitingTakeover`, où la portière et le frein de service peuvent être
actionnés pendant 60 secondes. Le moteur doit rester confirmé, la boîte en
`Park`, le frein de stationnement serré et les autres interverrouillages valides.
L'expiration entraîne `Stopping`. Seul un événement
`DriverTakeoverConfirmed`, produit par un futur adaptateur authentifié, permet
de libérer la commande distante vers `DriverControl`.

## Défenses matérielles attendues

Avant toute activation réelle, l'électronique doit au minimum prévoir :

- sorties désactivées au reset et en haute impédance si nécessaire ;
- protection contre inversion, surtension, transitoires et surintensité ;
- watchdog indépendant ;
- interverrouillage matériel empêchant le démarreur hors conditions ;
- retour d'état des actionneurs ;
- alimentation adaptée aux contraintes automobiles ;
- connectique et isolation appropriées ;
- procédure d'arrêt d'urgence accessible ;
- journalisation exploitable sans donnée sensible.

Une analyse de risques formelle et des essais HIL doivent compléter ces défenses.
Le rôle exact et les limites de la défense logicielle sont détaillés dans
[actuator-safety-supervisor.md](actuator-safety-supervisor.md).

## Hors périmètre actuel

- homologation et conformité réglementaire ;
- sécurité cryptographique de la commande distante ;
- compatibilité exacte par variante E9x ;
- diagnostic CAN/K-CAN/CAS/DME ;
- détection de présence humaine ou animale ;
- qualification BMW des preuves autorisant la reprise conducteur ;
- qualification BMW de la provenance et de l'anti-rejeu des verrouillages ;
- protection physique contre les erreurs de câblage.
