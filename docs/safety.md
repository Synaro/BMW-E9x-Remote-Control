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
réévaluées. Une ouverture surveillée (portes, coffre et capot lorsqu'il est requis),
un changement de rapport, un frein actionné, une perte
de fraîcheur ou un défaut critique mène à l'état `Fault` avec sécurisation des
sorties.

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

## Hors périmètre actuel

- homologation et conformité réglementaire ;
- sécurité cryptographique de la commande distante ;
- compatibilité exacte par variante E9x ;
- diagnostic CAN/K-CAN/CAS/DME ;
- détection de présence humaine ou animale ;
- prise de contrôle par la clé et transfert de session ;
- protection physique contre les erreurs de câblage.
