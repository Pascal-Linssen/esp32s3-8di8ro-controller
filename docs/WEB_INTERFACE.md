# Interface Web

## Accès

### URL
**http://192.168.1.50** (IP par défaut de l'ESP32)

### Fonctionnalités
- 🏭 **Contrôle industriel en temps réel**
- 📱 **Interface responsive** (mobile/tablette/desktop)
- 🔄 **Actualisation automatique** toutes les 10 secondes
- 🎨 **Interface moderne** avec thème industriel

## Sections de l'Interface

### 📊 État du Système
- **Ethernet** : Affichage de l'IP actuelle
- **MQTT** : État de la connexion au broker
- **Uptime** : Temps de fonctionnement en secondes

### 🔌 Contrôle des Relais
- **8 boutons individuels** pour chaque relais
- **Bouton "Basculer Tous"** pour inverser l'état de tous les relais
- **Couleurs visuelles** :
  - 🟢 **Vert** : Relais activé (ON)
  - 🔴 **Rouge** : Relais désactivé (OFF)
- **Contrôle instantané** : Clic = changement immédiat

### 📥 Entrées Digitales
- **Affichage en temps réel** des 8 entrées
- **Grille 4x2** pour visualisation optimale
- **États visuels** :
  - 🟢 **HIGH** : Entrée à l'état haut (3.3V)
  - 🔴 **LOW** : Entrée à l'état bas (0V)

### 🌡️ Capteurs
- **Température** : Affichage en °C (DHT22)
- **Humidité** : Affichage en % (DHT22)
- **Valeurs en temps réel** mises à jour automatiquement

## Utilisation

### Navigation
1. **Ouvrir un navigateur** web
2. **Aller à** : http://192.168.1.50
3. **L'interface se charge** automatiquement

### Contrôle des Relais
1. **Cliquer** sur le bouton du relais désiré
2. **Le relais change d'état** immédiatement
3. **La couleur du bouton** se met à jour
4. **L'état physique** du relais change sur la carte

### Monitoring
- **Les données se mettent à jour** automatiquement
- **Actualisation manuelle** : F5 ou rechargement de page
- **Aucune déconnexion** nécessaire

## API REST Intégrée

### Contrôle par URL
Vous pouvez contrôler les relais directement via URL :

#### Basculer un relais spécifique
```
http://192.168.1.50/relay?num=1&action=toggle
http://192.168.1.50/relay?num=2&action=toggle
...
http://192.168.1.50/relay?num=8&action=toggle
```

#### Basculer tous les relais
```
http://192.168.1.50/relay?action=all_toggle
```

### Intégration Externe
Ces URLs peuvent être utilisées avec :
- **Scripts Python/curl**
- **Home Assistant** (REST commands)
- **Node-RED** (HTTP requests)
- **Applications mobiles** personnalisées

#### Exemple curl
```bash
# Basculer relais 1
curl "http://192.168.1.50/relay?num=1&action=toggle"

# Basculer tous les relais
curl "http://192.168.1.50/relay?action=all_toggle"
```

#### Exemple Python
```python
import requests

# Basculer relais 3
response = requests.get("http://192.168.1.50/relay?num=3&action=toggle")
print(response.text)  # "OK"
```

## Personnalisation

### Thème/Couleurs
Dans `main.cpp`, fonction `generateWebPage()`, modifier le CSS :
```cpp
html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}";
// Changer #f0f0f0 pour couleur de fond différente
```

### Titre de la page
```cpp
html += "<title>ESP32-S3 Controller</title>";
// Changer "ESP32-S3 Controller" pour votre titre
```

### Fréquence d'actualisation
```cpp
html += "setTimeout(function(){location.reload();},10000);"; // 10 secondes
// Changer 10000 pour modifier la fréquence (en millisecondes)
```

## Performance

### Optimisations
- **CSS inline** pour réduire les requêtes
- **JavaScript minimal** pour performance
- **Actualisation intelligente** seulement si nécessaire
- **Réponses légères** pour les actions

### Limitations
- **1 client à la fois** recommandé pour performance optimale
- **Timeout** de 10 secondes pour les requêtes longues
- **Mémoire limitée** : interface simple et efficace

## Dépannage

### Interface non accessible
1. **Vérifier l'IP** : regarder les logs série pour l'IP actuelle
2. **Ping test** : `ping 192.168.1.50`
3. **Câble Ethernet** : vérifier la connexion physique
4. **Firewall** : s'assurer que le port 80 n'est pas bloqué

### Relais ne répondent pas
1. **Vérifier les logs série** : messages d'erreur I2C
2. **Test manuel** : utiliser commande série `relay X on/off`
3. **Scan I2C** : commande `scan` pour vérifier TCA9554

### Interface lente
1. **Réduire la fréquence** d'actualisation automatique
2. **Utiliser un seul onglet** à la fois
3. **Fermer les outils de développement** du navigateur

### Données non mises à jour
1. **Forcer l'actualisation** : Ctrl+F5
2. **Vider le cache** du navigateur
3. **Vérifier la connexion réseau**

## Exemples d'Intégration

### Home Assistant
```yaml
# configuration.yaml
rest_command:
  relay_1_toggle:
    url: "http://192.168.1.50/relay?num=1&action=toggle"
  
  all_relays_toggle:
    url: "http://192.168.1.50/relay?action=all_toggle"
```

### Node-RED
```json
[
  {
    "id": "http_request",
    "type": "http request",
    "method": "GET",
    "url": "http://192.168.1.50/relay?num={{payload}}&action=toggle"
  }
]
```