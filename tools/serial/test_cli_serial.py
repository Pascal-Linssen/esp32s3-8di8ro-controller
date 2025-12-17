#!/usr/bin/env python3
"""
Test des relais via CLI série
Valide que tous les relais répondent correctement via la liaison série
Ne dépend pas du broker MQTT
"""

import serial
import time
import sys
import re

# Configuration
PORT = "COM4"
BAUD = 9600
TIMEOUT = 2

def open_serial():
    """Ouvrir la connexion série"""
    try:
        ser = serial.Serial(PORT, BAUD, timeout=TIMEOUT)
        time.sleep(2)  # Attendre l'initialisation
        return ser
    except Exception as e:
        print(f"✗ Erreur ouverture port {PORT}: {e}")
        return None

def send_command(ser, cmd):
    """Envoyer une commande et attendre la réponse"""
    print(f"  → {cmd}")
    ser.write(f"{cmd}\n".encode())
    time.sleep(0.5)
    
    response = ""
    try:
        while ser.in_waiting > 0:
            response += ser.read(1).decode('utf-8', errors='ignore')
            time.sleep(0.05)
    except:
        pass
    
    return response

def parse_status(response):
    """Parser la ligne de statut"""
    # Format: "Temp=0.0°C Hum=0.0% | Relais: 0 0 0 0 0 0 0 0 | Entrées: 1 1 1 1 1 0 1 1"
    match = re.search(r'Relais: ([\d\s]+)', response)
    if match:
        return match.group(1).strip().split()
    return None

def test_individual_relays(ser):
    """Tester chaque relais individuellement"""
    print("\n" + "="*60)
    print("TEST 1: Contrôle des relais individuels")
    print("="*60)
    
    results = []
    for relay in range(8):
        print(f"\n>>> Test Relais {relay}")
        
        # Allumer
        resp = send_command(ser, f"relay {relay} on")
        time.sleep(0.5)
        status = send_command(ser, "")
        states = parse_status(status)
        
        if states and len(states) > relay and states[relay] == "1":
            print(f"  ✓ Relais {relay} ON - OK")
            results.append(True)
        else:
            print(f"  ✗ Relais {relay} ON - FAILED (réponse: {states})")
            results.append(False)
        
        # Éteindre
        resp = send_command(ser, f"relay {relay} off")
        time.sleep(0.5)
        status = send_command(ser, "")
        states = parse_status(status)
        
        if states and len(states) > relay and states[relay] == "0":
            print(f"  ✓ Relais {relay} OFF - OK")
            results.append(True)
        else:
            print(f"  ✗ Relais {relay} OFF - FAILED (réponse: {states})")
            results.append(False)
    
    return results

def test_all_relays(ser):
    """Tester tous les relais à la fois"""
    print("\n" + "="*60)
    print("TEST 2: Commande d'extinction globale")
    print("="*60)
    
    print("\n>>> Extinction de TOUS les relais...")
    send_command(ser, "relay all off")
    time.sleep(0.5)
    status = send_command(ser, "")
    states = parse_status(status)
    
    if states and all(s == "0" for s in states):
        print(f"  ✓ Tous les relais OFF - OK")
        return True
    else:
        print(f"  ✗ Tous les relais OFF - FAILED (réponse: {states})")
        return False

def test_help(ser):
    """Afficher l'aide"""
    print("\n" + "="*60)
    print("TEST 3: Commande HELP")
    print("="*60)
    
    response = send_command(ser, "help")
    if "relay" in response.lower():
        print(f"  ✓ HELP retourne les commandes disponibles")
        return True
    else:
        print(f"  ✗ HELP - aucune réponse valide")
        return False

def test_system_info(ser):
    """Afficher les infos système"""
    print("\n" + "="*60)
    print("TEST 4: Statut du système")
    print("="*60)
    
    status = send_command(ser, "")
    print(f"  Statut actuel: {status[:80]}")
    
    if "Temp" in status and "Relais" in status and "Entrées" in status:
        print(f"  ✓ Tous les champs de statut présents")
        return True
    else:
        print(f"  ✗ Format de statut incorrect")
        return False

def main():
    print("\n" + "="*60)
    print("TEST CLI SÉRIE - ESP32-S3-ETH-8DI-8RO")
    print("="*60)
    print(f"Port: {PORT}")
    print(f"Baud: {BAUD}")
    
    ser = open_serial()
    if not ser:
        print("✗ Impossible de se connecter au port série")
        return False
    
    print(f"✓ Port ouvert")
    
    all_results = []
    
    try:
        # Tests
        individual = test_individual_relays(ser)
        all_results.extend(individual)
        
        all_off = test_all_relays(ser)
        all_results.append(all_off)
        
        help_test = test_help(ser)
        all_results.append(help_test)
        
        system_test = test_system_info(ser)
        all_results.append(system_test)
        
        # Résumé
        print("\n" + "="*60)
        print("RÉSUMÉ DES TESTS")
        print("="*60)
        passed = sum(1 for r in all_results if r)
        total = len(all_results)
        print(f"\n✓ Tests réussis: {passed}/{total}")
        
        if passed == total:
            print("🎉 TOUS LES TESTS PASSÉS!")
            return True
        else:
            print(f"⚠️  {total - passed} test(s) échoué(s)")
            return False
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Tests interrompus")
        return False
    except Exception as e:
        print(f"\n✗ Erreur: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        ser.close()
        print("\n")

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
