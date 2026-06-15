/*  ============================================================
  Project: machine
  File:  functions.h
  Description: Controle une machine de test de cartilage 4 puits

  © 2026 Adrian Utge Le Gall
  All rights reserved.

  This software is provided for academic and research purposes only.
  Any reproduction, modification, or distribution of this software,
  in whole or in part, for commercial purposes is strictly prohibited
  without prior written permission from the author.

  Author: Adrian Utge Le Gall
  Contact: adrianutge@gmail.com - utgelega@etu.univ-grenoble-alpes.fr
  Created: 2026-04-13
 ============================================================
 */ 


int myFunction(int x, int y) {
  return x + y;
}

int helloworld() {
  return 0;
}


int hz_to_step(int frequency) {
  return (int)( (60 * frequency)/(0.229f) );

}