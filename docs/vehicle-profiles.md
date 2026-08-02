# Profils véhicule

## But

Un profil décrit une variante de véhicule sans introduire de détails de bus dans
le domaine. Le contrôleur raisonne toujours sur les mêmes signaux sémantiques :
régime moteur, batterie, capot, transmission, rapport, etc. Les futurs décodeurs
d'infrastructure seront responsables de produire ces signaux pour chaque
variante prise en charge.

Le projet n'est donc pas limité à une seule voiture. La première entrée du
registre est un véhicule de référence utile pour la découverte :

- BMW E90, année modèle 2009 ;
- moteur diesel N47D20C ;
- boîte automatique ;
- identifiant `bmw-e90-2009-n47d20c-automatic`.

Les E91, E92, E93, autres années, moteurs ou transmissions devront recevoir leur
propre profil quand leurs différences sont établies. Aucun profil ne doit être
déduit d'un autre uniquement parce que les véhicules semblent proches.

## État des signaux

Chaque signal d'un profil est classé :

| État | Signification |
|---|---|
| `Unavailable` | source absente ou non identifiée ; |
| `Candidate` | source possible, encore non validée ; |
| `Verified` | source et décodage validés selon le plan de test. |

Le profil E90 de référence place actuellement tous les signaux en `Candidate`.
Il ne contient aucun identifiant CAN BMW et ne prétend pas que les données sont
déjà décodées.

## Niveaux de qualification

Les niveaux progressent de `Discovery` à `ReadOnlyValidated`, puis
`BenchValidated` et enfin `VehicleQualified`. Le garde
`assessRemoteStartReadiness()` refuse un profil lorsque :

- un signal obligatoire manque ou reste candidat ;
- le niveau lecture seule n'est pas atteint ;
- la transmission est inconnue ;
- une boîte manuelle n'a pas reçu une autorisation de politique explicite.

Les portes et le coffre deviennent également obligatoires lorsque la politique
exige que le véhicule soit sécurisé. Le profil de référence est actuellement au
niveau `Discovery` : son évaluation échoue donc volontairement de manière sûre.

## Ajouter une variante

1. créer un profil séparé avec un identifiant stable ;
2. ne marquer que les caractéristiques établies ;
3. conserver les signaux non qualifiés en `Candidate` ou `Unavailable` ;
4. ajouter les tests de registre et de garde ;
5. qualifier séparément le décodeur lecture seule ;
6. ne promouvoir le niveau qu'avec des preuves de test conservées.

La sélection automatique d'un profil et la détection d'identité véhicule ne
font pas encore partie du firmware. En revanche, une sélection explicite via
`ControllerConfig::vehicleProfile` est obligatoire : un pointeur nul ou un
profil non qualifié fait refuser la demande en `Idle` avant tout accès véhicule.
Les tests et le simulateur emploient un profil synthétique qualifié qui ne doit
jamais être sélectionné dans une intégration réelle.
