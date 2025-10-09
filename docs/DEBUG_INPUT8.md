# 🔧 Guide de Dépannage - Entrée 8 Toujours Activée

## 🚨 Problème Observé
- **Symptôme** : L'entrée 8 (GPIO 11) affiche constamment "8:1" (état ACTIVÉ)
- **Comportement** : Ne change jamais d'état malgré les changements physiques

## 🔍 Diagnostic à Effectuer

### Étape 1: Diagnostic Automatique
Dans le moniteur série, tapez :
```
inputs
```

Cette commande va :
- Tester 10 lectures successives sur chaque entrée
- Vérifier la stabilité des signaux
- Tester les résistances de pullup
- Identifier si GPIO 11 est forcé à la masse

### Étape 2: Test Pin Alternative
```
fixinput8
```
Teste GPIO 21 comme alternative à GPIO 11

### Étape 3: Basculer vers Pin Alternative
```
switchinput8
```
Active GPIO 21 comme entrée 8 (nécessite déplacement physique du câble)

## 🔧 Solutions Possibles

### Solution 1: GPIO 11 Forcé à la Masse
**Cause** : Connexion physique défaillante ou court-circuit
**Solution** :
1. Vérifier les connexions sur GPIO 11
2. Mesurer avec multimètre : GPIO 11 doit être ~3.3V au repos
3. Déconnecter temporairement le signal d'entrée 8

### Solution 2: Conflit de Pin
**Cause** : GPIO 11 utilisé par une autre fonction système
**Solution** : Utiliser GPIO 21 comme alternative
```bash
# Dans le code, modifier :
#define INPUT_8   21  // Au lieu de 11
```

### Solution 3: Problème de Pullup
**Cause** : Résistance pullup interne défaillante
**Solution** : Ajouter résistance pullup externe 10kΩ

### Solution 4: Modification Hardware
**Option 1** : Déplacer physiquement le câble vers GPIO 21
**Option 2** : Utiliser un autre GPIO disponible

## 📊 Résultats de Diagnostic Attendus

### Normal (GPIO Fonctionnel)
```
Entrée 8 (GPIO 11):
  Lectures: HIGH=10 LOW=0 -> STABLE HIGH (pullup OK, entrée inactive)
  Test pullup: Sans=1 Avec=1 -> Pullup fonctionne
```

### Problématique (GPIO à la Masse)
```
Entrée 8 (GPIO 11):
  Lectures: HIGH=0 LOW=10 -> STABLE LOW (entrée activée)
  Test pullup: Sans=0 Avec=0 -> PIN FORCE A LA MASSE!
```

### Instable (Connexion Défaillante)
```
Entrée 8 (GPIO 11):
  Lectures: HIGH=6 LOW=4 -> INSTABLE! Problème de connexion
```

## ⚡ Solutions Rapides

### Solution Immédiate
```bash
# Dans le moniteur série :
switchinput8    # Utilise GPIO 21 temporairement
```

### Solution Permanente
Modifier le code pour utiliser définitivement GPIO 21 :
```cpp
#define INPUT_8   21  // Remplacer 11 par 21
```

## 🔌 GPIO Alternatifs Disponibles

| GPIO | Statut | Recommandation |
|------|--------|----------------|
| 21   | ✅ Libre | **Recommandé** - Testé |
| 22   | ✅ Libre | Alternative |
| 47   | ✅ Libre | Alternative |
| 48   | ✅ Libre | Alternative |

## 📝 Commandes de Test

```bash
help        # Liste toutes les commandes
status      # État actuel du système  
inputs      # Diagnostic complet des entrées
fixinput8   # Test GPIO 21 comme alternative
switchinput8# Basculer vers GPIO 21
testio      # Test complet entrées/sorties
```

## 🎯 Prochaines Étapes

1. **Exécuter** `inputs` pour identifier la cause exacte
2. **Tester** `fixinput8` pour valider GPIO 21
3. **Déplacer** physiquement le câble si nécessaire
4. **Confirmer** le bon fonctionnement avec `status`

## 📞 Support

Si le problème persiste après ces tests :
- Fournir la sortie complète de la commande `inputs`
- Indiquer les lectures obtenues sur GPIO 21
- Vérifier l'état physique des connexions