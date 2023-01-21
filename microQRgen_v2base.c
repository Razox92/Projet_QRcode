/////////////////////////////////////////////////////////////////////////////////
/// MicroQRgen
/// \file microQRgen_v2base.c
/// \brief Générateur de microQRcode M1/M2/M3/M4 selon la norme ISO18004/2015
/// \author Stéphane BRETTE / IUT Ville d'avray
/// \version 2.0
/// \date 28 décembre 2021
/////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

// definitions de la taille des microQRcode M1/M2/M3/M4
// 1 module = 1 petit carré blanc ou noir composant le QRcode
// remarque :  1 module peut donner 8x8 pixels ou plus sur l'image du QRcode (voir la definition de PIX_BY_MODULE)
                                                                //
#define NB_MODULE_M1  11      /** largeur d'un QRCode dans le mode M1  */
#define NB_MODULE_M2  13      /** largeur d'un QRCode dans le mode M2  */
#define NB_MODULE_M3  15      /** largeur d'un QRCode dans le mode M3  */
#define NB_MODULE_M4  17      /** largeur d'un QRCode dans le mode M4  */

#warning selectionner la taille des qrcode selon le mode choisi ci dessous
#define NB_MODULE     NB_MODULE_M4    /// Taille d'un module ( A SELECTIONNER)

//  versions pour microQRcode (voir ISO 18004/2015 p57
//  ces versions sont embarques dans le codage de version(et utilisées aussi pour le codage du flux binaireà
#define M1_       0b000          /** microQR M1 sans code de correction      */
#define M2_L      0b001          /** microQR M2 avec code Low correction     */
#define M2_M      0b010          /** microQR M2 avec code Medium correction  */
#define M3_L      0b011          /** microQR M3 avec code low correction     */
#define M3_M      0b100          /** microQR M3 avec code medium correction  */
#define M4_L      0b101          /** microQR M4 avec code low correction     */
#define M4_M      0b110          /** microQR M4 avec code medium correction  */
#define M4_Q      0b111          /** microQR M4 avec code quality correction */

// modes d'encodages disponibles
// Voir ISO18004/v2015 table 3 et 4 p23 pour les differents modes utilisables selon M1/M2/M3/M4

#define NUMERIC   0b0001    /** mode numerique  (supporté par M1/M2/M3/M4)              */
#define ALPHANUM  0b0010    /** mode alphanum a 45 caractere  (supporté par M2/M3/M4)   */
#define ASCII     0b0100    /** mode ascii  supporté par M3/M4                          */
#define KANJI     0b1000    /** mode kanji , pour complétude mais non implémenté        */

// definition du noir et du blanc
#define NOIR      0         /**  module de couleur nNOIRE   */
#define BLANC     255       /**  module de couleur BLANCHE  */

//////////////////////SUJET 0 = COMMUN SUJETS 1 2 3 4/////////////////////
// fonctions communes
void efface_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]);                  // sujet0 (commun) :  Efface  un QRcode  (tous les modules = BLANC = 255)
void initialise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]);              // Sujet 0 (commun) : initialise les finder pattern et synchronous pattern

// /////////////////// SUJET 1 ///////////////////////////////////////////
// gestion des version/mode et n° mask
unsigned short int  encode_version(int type, int no_masque);   // sujet 1 : combine n° de type et n° de masque, ajoute la protection BCH et masaque le tout
void ajoute_version_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned short int header15bits);  // sujet1 :  ecrit les 15 bits de version + code corretcion

////////////////////// SUJET 2 ///////////////////////////////////////////
// fonctions autour du masquage 2D
void genere_QRmask(unsigned char qrmask[NB_MODULE][NB_MODULE],int no_masque);     // sujet 2 : genere un QRmask (n° entre 0 et 3)
void Black_And_Whitise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]);        // sujet 2 :  sature en noir tout ce qui n'est pas blanc
void xor_QRcode_QRmask(unsigned char qrcode[NB_MODULE][NB_MODULE],const unsigned char qrmask[NB_MODULE][NB_MODULE]);  //sujet 2:  Réalise le masquage du QRcode
int  score_masquage_QRcode(const unsigned char qrcode[NB_MODULE][NB_MODULE]);     // sujet 2(complement) calcul la metrique pour choix du meilleur masque

// /////////////////// SUJETS 3 //////////////////////////////////////////
//Fonctions d'encodage du datastream
// encode la chaine de données dans le datastream binaire
// 3 fonctions suivant que la chaine initiale doit etre encodée en mode numerique (chiffre de 0 a 9 uniquement)
// alphanumeriqe (45 caractere) ou ascii
// + 1 fonction d'aiguillage
int  numeric_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version);   // Sujet 3c : Difficile
int alphanum_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version);   // sujet 3b : Très difficile
int    ascii_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version);   // sujet 3a : moyennement difficile

int data_string_to_binaryDS(const unsigned char data_string[24+1],  // sujets 3a,b,c (facultatif)  : aiguillage vers les fonctions xxx_to_binary_DS
                                  unsigned char binaryDS[24*8],
                             unsigned short int version,
                             unsigned short int mode );

void binaryDS_to_packedbyteDS(const unsigned char binaryDS[24*8], unsigned char packedbyteDS[24],unsigned short int version); // sujets 3a,b,c ! recompaction en bloc de 8 (sauf M1/M3 qui comportent 1 bloc de 4
                                                                                                                            // pour écriture sur le QRCODE avec put_byte_in_block
// /////////////////// SUJETS 4   ////////////////////////////////////////////
// definition des 4+1 types de blocks de 8 modules dans un microqrcode
#define UP 1                      /** orientation de bas en haut (UP)   pour un block de 8 bits */
#define DN 2                      /** orientation de huat en bas (DowN) pour un block de 8 bits */
#define UL 3                      /** orientation de droite a gauche avec debut en bas (Up, Left) par exemple : bloc  4 en M3L */
#define DL 4                      /** orientation de droite a gauche avec debut en haut(Down, Left) par exemple : bloc 13 en M3L */
#define US 5                      /** Up Special pour bloc de 4 bits n°3 M1, blocs n°9 et 13 M3 */

// ecriture d'un bloc de 8 modules a un emplacement donné (et direction donnée UP/DN/UL,DL, US ... )
void put_byte_in_blocks(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned char byte,      // Sujets 4 (commun a toutes les versions M1/M2/M3/M4)
                        unsigned char lig, unsigned char col, unsigned char direction);

// fonctions spécifiques à l'ecriture du datastream dans la zone de donnée des QR code
void ajoute_dataM1_QRcode( const unsigned char packedbyteDS[24], unsigned char qrcode[NB_MODULE][NB_MODULE]); // Sujet4/M1  : ajoute les données (packed) a un QR code de type M1
void ajoute_dataM2_QRcode( const unsigned char packedbyteDS[24], unsigned char qrcode[NB_MODULE][NB_MODULE]); // Sujet4/M2  : ajoute les données (packed) a un QR code de type M2
void ajoute_dataM3L_QRcode(const unsigned char packedbyteDS[24], unsigned char qrcode[NB_MODULE][NB_MODULE]); // HORS SUJET : ajoute les données (packed) a un QR code de type M3L
void ajoute_dataM3M_QRcode(const unsigned char packedbyteDS[24], unsigned char qrcode[NB_MODULE][NB_MODULE]); // HORS SUJET : ajoute les données (packed) a un QR code de type M3M
void ajoute_dataM4_QRcode( const unsigned char packedbyteDS[24], unsigned char qrcode[NB_MODULE][NB_MODULE]); // Sujet4/M4  : ajoute les données (packed) a un QR code de type M4

////////////////////////////ROUTINES d'AFFFICHAGE ET DE TEST FOURNIES /////////////////////////////////////
// routines pour creer le fichier image en mode PGM (portable gray map) ou PPN (portable pixmap
int  QRcode_to_pgm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename);                          // code C fourni :ecrit un QRcode dans un fichier PGM (couleur)
int  QRcode_to_ppm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename, unsigned long int color); // code C fourni : ecrit un QRcode dans un fichier PPM (N&B)

// affichages console qrcode/chaine a encoder/binarystrema
void QRcode_to_console(    const unsigned char qrcode[NB_MODULE][NB_MODULE]);                   // code C fourni : affiche un QRcode sur la console
void datastring_to_console(const unsigned char datastring[]);                                   // code C fourni : affiche une datastring sur la console
void binaryDS_to_console(  const unsigned char binaryDS[]);                                     // code C fourni : affiche une bianry DataString  sur la console
void packedbyte_to_console(const unsigned char packedbyteDS[]);                                  // code C fourni : affiche une packedbinary sur la console

//////////////////// TESTS UNITAIRES
// les delcarations des fonctions de tests unitaires pour chaque sujet
void test_unitaire_sujet0(void);
void test_unitaire_sujet1(void);
void test_unitaire_sujet2(void);
void test_unitaire_sujet3(void);
void test_unitaire_sujet4(void);

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//////le MAIN !!!!!!!!!!!!!!///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief le MAIN !!!!

int main(void){

   test_unitaire_sujet0();
   //test_unitaire_sujet1();
   //test_unitaire_sujet2();
   //test_unitaire_sujet3();
   //test_unitaire_sujet4();

    return 0;
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet0(void)
///\brief exemple de test unitaire pour le sujet 0
///
void test_unitaire_sujet0(void){
  unsigned char MicroQRcode[NB_MODULE][NB_MODULE];  // le microQRcode sans la Quiet ZONE

  // affichage a la console du QRcode
    printf("\n ********************************************");
    printf("\n Test unitaire 0 : MicroQRcode non initialisé \n");
    QRcode_to_console(MicroQRcode);

  // exemple de positionnement de 4 MODULES BLANCS
    MicroQRcode[3][8] = BLANC;  // test
    MicroQRcode[3][9] = BLANC;  // Un petit carré 2x2 de modules BLANCS
    MicroQRcode[4][8] = BLANC;  //
    MicroQRcode[4][9] = BLANC;  //

    printf("\nTest unitaire 0 : MicroQRcode non initialisé avec 4 modules BLANCS \n");
    QRcode_to_console(MicroQRcode);

    // effacement du microQRcode ==>100% BLANC et verification
    efface_QRcode(MicroQRcode);
    printf("\nTest unitaire 0 : MicroQRcode 100% BLANC \n");
    QRcode_to_console(MicroQRcode);

    // initialisation eu microQRcode (finder patterne + synchro pattern) + verification
    initialise_QRcode(MicroQRcode);

    printf("\nTest unitaire 0 : MicroQRcode initialisé ave ses finder et synchronous patterns \n");
    QRcode_to_console(MicroQRcode);
    printf("\n Test unitaire 0 : Export du microQRcode dans images/Mon_premierQRcode.pgm\n");
    QRcode_to_pgm(MicroQRcode,"images/Mon_premierQRcode.pgm");
    //QRcode_to_ppm( MicroQRcode, "images/Mon_premierQRcode.pgm",0x00FF0000);  // en rouge

    printf("\n***************** fin Test unitaire 0 *****************");
    printf("\n*******************************************************\n\n\n");
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet1(void)
///\brief tests unitaire pour le sujet 1
///
void test_unitaire_sujet1(void){
     unsigned char MicroQRcode[NB_MODULE][NB_MODULE];  // le microQRcode sans la Quiet ZONE
     unsigned short int mode_microQRcode = M4_L ;    // choix du mode (
     unsigned short int mask_number = 2;             // choix du n° de MASK
     unsigned short int header15bits_microQRcode  ;  // header = mode + numero de masque + 10 bits BCH = 15 bits

    // A COMPLETER
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet2(void)
///\brief tests unitaire pour le sujet 2
///
void test_unitaire_sujet2(void){
     unsigned char MicroQRcode[NB_MODULE][NB_MODULE];  // le microQRcode sans la Quiet ZONE
     unsigned char MicroQRmask[NB_MODULE][NB_MODULE];  // le mask

     unsigned short int mode_microQRcode = M4_L ;    // choix du mode
     unsigned short int mask_number = 2;             // choix du n° de MASK
     int score;
    // A COMPLETER
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet3(void)
///\brief tests unitaire pour le sujet 3
///
void test_unitaire_sujet3(void){
      unsigned char data_string[24+1]="Bonjour IUT";          // pour test ASCII
      //unsigned char data_string[24+1]="534956";             // un autre pour test de l'encodage numérique ( 2 triplets de chiffres)
      //unsigned char data_string[24+1]="TEXTE +/1234";       //un autre pour test de l'encodage alphanum a 45 car (6 doublets de 2 caracteres)

      unsigned char binaryDS[24*8]={1,0,1,1,1,0,1,1,    // chaine binaire test de 17 bits
                                    0,1,0,1,1,0,0,0,
                                    0, 255 } ;         // (255 = fin de chaine binaire)
      unsigned char packedbyteDS[24];      //Flux binaire compacté en paquet de 8 bits (initalisé uniquement pour les tests)
      unsigned short int mode_microQRcode = M4_L ;    // choix du mode (
      unsigned short int mask_number = 2;             // choix du n° de MASK

      printf("Affichage de la binary data string \n ");
      binaryDS_to_console(binaryDS);
         // A COMPLETER
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet4(void)
///\brief tests unitaire pour le sujet 4
///
void test_unitaire_sujet4(void){
     unsigned char MicroQRcode[NB_MODULE][NB_MODULE];  // le microQRcode sans la Quiet ZONE
     unsigned char packedbyteDS[24] ={0xFF,0xAA,0xFE,0x11,0x00,0x53,0x72,0xF0,0xFE};

     unsigned short int mode_microQRcode = M4_L ;    // choix du mode (

     // A COMPLETER
     printf("Un block de 8 bits ecrit dans le QRcode ");
     QRcode_to_console(MicroQRcode);
}
// FIN DES TESTS UNITAIRES
//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/// \fn void efface_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
/// \brief Sujet 0 (commun tous sujets 1,2,3,4) :  efface entierement un qrcode (ou un qrmask) et positione tous ses modules a 255 (BLANC)
/// \param[out] qrcode[][]   qrcode (ou qrmask) a effacer
void efface_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i,j;
     // A COMPLETER
}

///////////////////////////////////////////////////////////////////
/// \fn void initialise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
/// \brief Sujet 0 (commun tous sujets 1,2,3,4) : initialise les pattern  obligatoires d'un QRCODE
///             finder pattern de 7x7 module en haut a gauche
///            + synchonous pattern coté haut et coté gauche
///             le qrcode doit etre préalablement effacé (100% blanc)
/// \param[in,out] qrcode[][]    qrcode à initialiser

void initialise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i,j;
    // A COMPLETER
}

///////////////////////////////////////////////////////////////////////////
/// \fn genere_QRmask(unsigned char qrmask[NB_MODULE][NB_MODULE],int no_masque)
/// \brief Sujet2 : Generation des qrmask pour masquage final une fois le qrcode complet
/// \param[out] qrmask[][] : le qrmak a remplir
/// \param[in] no_masque  : n° de masque de 0 a 3 (cf masque dans ISO18004/2015 p52 et 54)

void genere_QRmask(unsigned char qrmask[NB_MODULE][NB_MODULE],int no_masque){
    //int i,j;

}

///////////////////////////////////////////////////////////////////////
/// void Black_And_Whitise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief  Code C fourni : cette fonction transforme tout ce qui n'est pas BLANC  (=255) en NOIR (=0) ,
///         Pour le debug, on a utilisé les majuscules F, S, V... pour reperer les zone finder, synchro, version
/// \param[in,out] qrcode[][] : le qrcode a mettre en noir et blanc
void Black_And_Whitise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
    int i,j;
    for(i=0;i<NB_MODULE;i++){
        for(j=0;j<NB_MODULE; j++){
            if(qrcode[i][j] < BLANC){
                qrcode[i][j] = NOIR; // tout ce qui n'est pas blanc est NOIR !
            };                      // cad : efface le detail des zone finder/syncrho Version
        }
     }

}
///////////////////////////////////////////////////////////////////////
/// void void xor_QRcode_QRmask(unsigned char qrcode[NB_MODULE][NB_MODULE],const unsigned char qrmask[NB_MODULE][NB_MODULE])
/// \brief Sujet2 :  Réalise le masquage/demasquage du QRcode avec le masque fourni
/// \param[in,out] qrcode[][] : le qrcode a masquer
/// \param[in] qrmask[][] : le qrmask a utiliser

void xor_QRcode_QRmask(unsigned char qrcode[NB_MODULE][NB_MODULE],const unsigned char qrmask[NB_MODULE][NB_MODULE]){
    //int i,j;
    // A COMPLETER
}
////////////////////////////////////////////////////////////////
/// \fn int score_masquage_QRcode(const unsigned char qrcode[NB_MODULE][NB_MODULE]){
/// \brief  Sujet 2 (facultatif)  : calcul du score apres masquage (ISO18004 page papier 53/54)
/// le principe est de tester les 4 masques definis pour les microQRcode et de choisir celui qui maximise ce score.
/// dans un 1er temps on n'optimisera pas ce masquage, mais on appliquera un masque fixe au choix du programmeur
///  toutefois la fonction de calcul du score est ci dessous
/// \param[in] qrcode[][] : le qrcode dont on doit calculer le score
/// \return Le score calculé

int score_masquage_QRcode(const unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int som_1=0, som_2=0,
    int score=0;
    //A COMPLETER (Facultatif)
    return score;
}
/////////////////////////////////////////////////////////////////////////
/// \fn unsigned short int  encode_version(int type, int no_masque)
/// \brief Sujet 1 : Fonction qui calcul les  15 bits du n° d'encodage  a partir du mode et
///             du n° masque (0 1 2 3) et ajoute les 10 bits de code BCH
/// \param[in] type : parmi M0_ (0) a M4_Q(7)
/// \param[in] no_masque : n° du masque utilisé (entre 0 et 3
/// \return  l'entete 15 bits a ecrire sur les 15 modules situés autour du finder pattern

// On a donc 3 bits de mode precisant M1_/M2_L/M2_M ...  et 2 pour l'un des 4 mask 2D
// le code BCH est lu depuis la table (iso18004/2015, page 80) ajouté bit14..bit5
// puis le tout est masqué (iso18004/2015 page 57)
//
// appel header = encode_version(M4_L, 2);
                                                                                        //
unsigned short int  encode_version(int type, int no_masque){
    unsigned short int header15bits  = 0;
    // A COMPLETER
return header15bits;
}

////////////////////////////////////////////////////
/// \fn void ajoute_version_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned short int header15bits){ajoute l'information de version sur le QRCODE
/// \param[in,out]  qrcode[][]  LE qrcode !
/// \param[in] : header15bits   entete a ecrire (version + n°mack + 10 bits BCH) précalculées
/// \brief Sujet 1 :  ajoute les 15 bits de version (deja calculés) dans la zone de 15 bits reservées qui entourent le FINDER PATTERN

void ajoute_version_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned short int header15bits){
//A COMPLETER
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn int numeric_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version)
/// \brief Sujet 3c (version M1) : Encode une datastring de type numérique  en chaine binaire (dont entete longuer/mode)
 ///      OBLIGATOIRE : longueur de chaine multiple de 3, facultatif (toute longueur)
/// \param[in]  data_string[] Chaine de caracteres a encoder dans le QRcode
/// \param[out] binaryDS[]    Chaine binaire encodée
/// \param[in]  version parmi M1_, M2_L, M2_M, M3....
///         toutes les versions (M1,M2,M3,M4) supportent le mode numérique
/// \return Retourne la longueur du datastream ou -1 si mode non supporté


int numeric_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version){
    //int index_s =0; // index de la chaine de caractere a encoder
    //int index_b =0; // index de la chaine binaire à remplir
    //int nb_chiffres;        // nombre de chiffres dans la chaine numérique a encoder
    int binaryDS_length;    // longueur du flux binaire

    return binaryDS_length;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn int alphanum_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24µ8], unsigned short int version){
/// \brief  Sujet 3b (version M2) : encode une datastring de type alphanumérique (45 lettres/chiffres/signes)  en chaine binaire (dont entete longuer/mode)
///         Obligatoire longueur de chaine multiple de 2, facultatif (toute longueur)
/// \param[in] data_string[] Chaine de caracteres a encoder dans le QRcode
/// \param[out] binaryDS[]    Chaine binaire encodée
/// \param[in] version parmis M1_, M2_L, M2_M, M3....
///         ATTENTION M1 ne supporte pas le mode alphanumérique
/// \return Retourne la longueur du datastream ou -1 si mode non supporté

int alphanum_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version){
    //int index_s =0; // index de la chaine de caractere a encoder
    //int index_b =0; // index de la chaine binaire à remplir
    int binaryDS_length;    // longueur du flux binaire
    //unsigned char car1, car2;          // 2 caracteres consécutif (parmi 45)
    //unsigned int double_car_alphanum;  // forment un double caractere encodé sur 11bits (0 a 2047

    // A COMPLETER

   return binaryDS_length;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \fn int ascii_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version){
/// \brief  Sujet 3a (version M4) : Encode une datastring de type ascii  en chaine binaire (dont entete longueur/mode)
/// \param[in]  data_string[] Chaine de caracteres a encoder dans le QRcode
/// \param[out] binaryDS[]    Chaine binaire encodée
/// \param[in]  version parmis M1_, M2_L, M2_M, M3....
///         ATTENTION M1 et M2  ne supportent pas le mode ascii
/// \return Retourne la longueur du datastream ou -1 si mode non supporté

int ascii_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version){
    //int index_s = 0; // index de la chaine de caractere a encoder
    //int index_b = 0; // index de la chaine binaire à remplir
    int binaryDS_length = 0 ;    // longueur du flux binaire

   //A COMPLETER
   return binaryDS_length;
}

// ////////////////////////////////////////////////////////////////////
/// \fn int data_string_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version, unsigned short int mode );
/// \brief Facultatif sujets3 :encode une chaine de caractere en binary datstring selon la version (M1_...) et le mode (num, alphanum, ascii) choisis
///        aiguillage vers l'une des 3 xxxx_to_binaryDS()inclus le controle d'erreur
/// \param[in]  data_string[] : chaine de caractere a encoder
/// \param[out] binaryDS[]    : chaine binaire encodée (incluant l'entete)
/// \param[in]  version : parmi M1_, M2_L, ... M3_Q
/// \param[in]  mode : parmi NUMERIC, ALPHANUM, ASCII
/// \return -1 si incompatibilité entre le mode et la version, nb de bit dans la chaine binaire si non
int data_string_to_binaryDS(const unsigned char data_string[24+1],
                                  unsigned char binaryDS[24*8],
                             unsigned short int version,
                             unsigned short int mode ){
            // TODO : controle de cohérence mode/version et aiguillage vers l'eune des 3 fonctions


return 0;
}

// ////////////////////////////////////////////////////////////////////
/// \fn  void binaryDS_to_packedbyteDS(const unsigned char binaryDS[], unsigned char packedbyteDS[],unsigned short int version)
/// \brief Tous sujets 3 (3a,3b,3c) :Pack un binary DS en packed byte (gere le cas particulier du bloc de 4bits M2/M3L/M3M
/// \param[in]   binaryDS[], la chaine binaire a compacter
/// \param[out]  packedbyteDS[24] : le tableau du flux binaire a ecrire compacté en octets (seuls les 5 premiers sont ecrits
/// \param[in]   version parmi M1_, M2_L .... M3_Q
///              ATTENTION : en M1 et M3, le bloc specifique de 4 bit est calé sur le MSB (bit7 a 4) de l'octet


void binaryDS_to_packedbyteDS(const unsigned char binaryDS[24*8], unsigned char packedbyteDS[24],unsigned short int version){
      //int i,index_binDS=0,bit;
      //unsigned char byte=0;
      int nb_packed[8] ={ 5,10,10,17,17,24,24,24};//
      int longueur_packed;
      longueur_packed = nb_packed[version]; // "calcul de la longueur"                                                              ;;
      // A completer
}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM1_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief Sujet 4 version M1 : Ajoute les données (packed) dans un microQRcode M1
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 5 premiers sont ecrits
/// \param[in,out] qrcode[][]  : le qrcode
void ajoute_dataM1_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i;
    // A COMPLETER
}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM2_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief Sujet 4 version M2 : Ajoute les données (packed) dans un microQRcode M2
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 10 premiers sont ecrits
/// \param[in,out] qrcode[][]  : le qrcode

void ajoute_dataM2_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i;
    // A completer
}
//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM3L_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief HORS SUJET  M3L : Ajoute les données (packed)  dans un microQRcode M3L
/// \param[in]      packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 17 premiers sont ecrits
/// \param[in,out]  qrcode[][]  : le qrcode

void ajoute_dataM3L_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i;
    // NON TRAITE
}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM3M_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief HORS SUJET  M3M : ajoute les données (packed) dans un microQRcode M3M
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 17 premiers sont ecrits
/// \param[in,out] qrcode[][]  : le qrcode !

void ajoute_dataM3M_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i;
    // NON TRAITE

}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM4_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief Sujet 4 (version M4) : Ajoute les données (packed) dans un microQRcode M4
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets
/// \param[in,out] qrcode[][] : le qrcode

void ajoute_dataM4_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE]){
    //int i;
    // A COMPLETER
}

////////////////////////////////////////////////////////////////////////////////////
/// \fn void put_byte_in_blocks(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned char byte,
///                        unsigned char lig, unsigned char col, unsigned char direction)
/// \brief Sujets 4 (commun a toutes les versions M1/M2/M3/M4 : Ecriture d'un bloc de 8 modules a un emplacement donné (et pour une direction donnée parmi UP, DN UL,DL, US  )
/// \param[in,out] qrcode[][] le qrcode
/// \param[in]    byte : les 8 bits à ecrire
/// \param[in]    lig :  la ligne ou doit etre placé le bit7
/// \param[in]   col  ; la colonne ou doit etre placée le bit de poid fort (bit7)
/// \param[in]    direction : la direction parmi ( UP, DN, UL, DL, et US  pour UP LEFT ou DOWN LEFT, et US pour les blocs spéciaux de 4 bist )

void put_byte_in_blocks(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned char byte,
                        unsigned char lig, unsigned char col, unsigned char direction){
          // A COMPLETER
}//

/////////////////////////////////////////////////////////////////////////
/// \fn void QRcode_to_console(const unsigned char qrcode[NB_MODULE][NB_MODULE]){
///  \brief  Code C fourni  : Fonction d'affichage basique du microQRcode sur la console
/// affiche un microQRcode sur la console, . pour les blanc, * pour les noirs, F,V, S pour reperer les differentes
/// zone lors du debug.
/// \param[in] qrcode : le QRcode a afficher sur la console !
void QRcode_to_console(const unsigned char qrcode[NB_MODULE][NB_MODULE]){
    int i,j;
    putchar('\n');
    for(i=0;i<NB_MODULE;i++){
        for(j=0;j<NB_MODULE;j++){
            switch(qrcode[i][j]){                                     // on ajoute 1 espace entre module pour que ca ressemble a un carré
                case BLANC :  putchar('.');putchar(' '); break;       // des . pour le blanc /
                case NOIR  :  putchar('*');putchar(' '); break;       // des * pour les noirs
                default    :  if((qrcode[i][j] <=126) && (qrcode[i][j]>= 32) ){ // on affiche les carectere "affichable"
                                    putchar(qrcode[i][j]);putchar(' ');
                              }else{
                                    putchar(' ');                      // si non on n'affiche rien
                                    putchar(' ');
                              };
                              break;
            }
        }
        putchar('\n');
    }
    putchar('\n');
}
///largeur en pixel pour un module, utilisé pour generer les fic image ppm/pgm
#define PIX_BY_MODULE 8
/////////////////////////////////////////////////////////////////////////
/// \fn int QRcode_to_pgm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename)
/// \brief Code C fourni : Fonction d'export en PGM (portable Gray map); format d'image lisible avec xniew
/// \param[in] qrcode[][] le QRcode !
/// \param[out] *filename     le nom du fichier image ou enregistre le qr code (avec extension .pgm)

// la constante précédente PIX_BY_MODULE, definit la taille en pixels d'un module dans le fichier PGM
// exemple si PIX_BY_MODULE =8, chaque module =1bit)= deviendra 8 x 8 pixels dans l'image
// le qrcode passé doit etre codé en niveau de gris (0 pour le noir, 255 pour le blanc)..

int QRcode_to_pgm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename){
	FILE *fd;
	int i,j,ii,jj;

//Test sur l'ouverture du fichier
	if(!(fd = fopen(filename,"w+")) ){
		fprintf(stderr,"QRcode2ppm, erreur de cr�ation du fichier %s\n",filename);
		exit(-1);
	};
// ecriture du PGM en niveau de gris (256) Entete d'un fichier ppm  P5 = Magick number....
       fprintf(fd,"P5\n");              // Magik number pour Portable GreyMap ==P5
       fprintf(fd,"#fichier PGM pour QRcode \n#IUT VDA S.BRETTE 2021\n");
       fprintf(fd,"%d %d 255 ",NB_MODULE*PIX_BY_MODULE,NB_MODULE*PIX_BY_MODULE);
                                    // entete ppm P5 : largeur, hauteur, nb niveaux de gris
                                      // NE PAS TOUCHER A CETTE ENTETE, espace (et non \n indispensable apres le 255
// BBBBBB a la suite jusqu'a la fin en partant en haut a gauche de l'image
	for(i=0;i<NB_MODULE;i++){
        for(ii=0;ii<PIX_BY_MODULE;ii++){ // on recopie plusieurs fois le module dans 8 pixels*8pixels
            for(j=0;j<NB_MODULE;j++){
                for(jj=0;jj<PIX_BY_MODULE;jj++){
                    fprintf(fd,"%c",qrcode[i][j]);
                }
            }
        }
	}
	fclose(fd);
	return 0;
}

/////////////////////////////////////////////////////////////////////////
/// /////////////////////////////////////////////////////////////////////////
/// \fn int QRcode_to_ppm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename, unsigned long int color)
/// \brief Code C fourni : Fonction d'export en PPM (portable Pix map  map); format d'image lisible avec xniew
/// \param[in]  qrcode[][]  LE qrcode !
/// \param[in] *filename     le nom du fichier image ou enregistre le qr code (avec extension .ppm)
/// \param[in]  color ! la couleur des modules qui ne sont pas blancs ( (0x00FF0000 pour le rouge pur)
/// la constante PIX_BY_MODULE, definit la taille en pixel d'un module dans le fichier PGM
/// exemple si PIX_BY_MODULE =8, chaque module deviendra 8 x 8 pixel dans l'image
/// le qrcode passé  est codé en niveau de gris (0 pour le noir, 255 pour le blanc)..
/// tout module == 0 sera ecrit en blanc, les autres dans la couleur RGB définit dans la fonction.
int QRcode_to_ppm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename, unsigned long int color){
	FILE *fd;
	int i,j,ii,jj;
	unsigned char red_level,green_level,blue_level; // RGB pour la couleur des pixels
	int w = NB_MODULE*PIX_BY_MODULE ; // surechantillonnage
	int h = NB_MODULE*PIX_BY_MODULE ; // surechantillonnage
    red_level   = (color & 0x00FF0000)>>16;     // extraction des couleurs primaires R,G,B
    green_level = (color & 0x0000FF00)>>8;      // pour ecriture dans le PPM
    blue_level  = (color & 0x000000FF);

//Test sur l'ouverture du fichier
	if(!(fd = fopen(filename,"w+")) ){
		fprintf(stderr,"QRcode2ppm, erreur de cr�ation du fichier %s\n",filename);
		exit(-1);
	};

// Entete d'un fichier ppm  P6 = Magick number....
       fprintf(fd,"P6\n%d %d\n255 ",w,h);  // entete ppm P6 : largeur, hauteur, nb niveaux sur  canaux RGB

// RGBRGBRGBRGB a la suite jusqu'a la fin en partant en haut a gauche de l'image
	for(i=0;i<NB_MODULE;i++){
        for(ii=0;ii<PIX_BY_MODULE;ii++){ // on recopie plusieurs fois le module dans 8 pixels*8pixels
            for(j=0;j<NB_MODULE;j++){
                for(jj=0;jj<PIX_BY_MODULE;jj++){ // repetition par colonne
                    if(qrcode[i][j] == BLANC){
                        fprintf(fd,"%c%c%c",255,255,255);  //
                    }else{
                       fprintf(fd,"%c%c%c",red_level, green_level, blue_level);
                    }
                }
            }
        }
    }
	fclose(fd);
	return 0;
}

////////////////////////////////////////////////////////////////
/// \fn void datastring_to_console(const unsigned char datastring[])
/// \brief Code C fourni : Affiche une datastring  Data stream sur la console

/// \param[in] datastring[] : la chaine de caractere a encoder dasn le QRcode
void datastring_to_console(const unsigned char datastring[]){
    int index = 0;
    do{
        putchar(datastring[index++]);
    }while((index < 24) && (datastring[index] != '\0')); // \0 en fin de chaine (sauf oubli)
    putchar('\n');
}

//////////////////////////////////////////////////////////////
/// \fn  void binaryDS_to_console(const unsigned char binaryDS[])
/// \brief Code C fourni : affiche un flux binaire sur la console (32 bits/ligne)
/// \param[in] binaryDS[] : la chaine binaire (255 = 0xFF = finde chaine binaire
/// rappel, un binary string est un tableau de 0 et 1 . la fin est marquée par la valeur 255
void binaryDS_to_console(const unsigned char binaryDS[]){
    int index = 0;

    do{
        if(index%32 == 0) putchar('\n');   // 4 octets par ligne
        if(index%8 == 0)  printf(" 0b");   // 0b devant chaque
        if(binaryDS[index] == 0){
            putchar('0');
        }
        if(binaryDS[index] == 1){
            putchar('1');
        }
        index++;
    }while((index < 24*8) && (binaryDS[index] < 255));
    putchar('\n');
}

////////////////////////////////////////
/// \fn  void packedbyte_to_console(const unsigned char packedbyteDS[])
/// \brief Code C fourni : Affiche un packedbyte Data stream sur la consoel
/// \param[in] packedbyteDS[] : le tableau d'octet a ecrire

void packedbyte_to_console(const unsigned char packedbyteDS[]){
    int index = 0 ;
    putchar('\n');
    do{
        printf("0x%x ",packedbyteDS[index++]);
        if(index%8 == 0) putchar('\n');
    }while((index<24) && (packedbyteDS[index] < 255));
    //#warning 'bug a corriger : si un packedbyte = 255 .... fin d'affichage'
    putchar('\n');
}

