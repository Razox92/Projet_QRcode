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
#include <string.h>
#include <math.h>

// definitions de la taille des microQRcode M1/M2/M3/M4
// 1 module = 1 petit carré blanc ou noir composant le QRcode
// remarque :  1 module peut donner 8x8 pixels ou plus sur l'image du QRcode (voir la definition de PIX_BY_MODULE)
//
#define NB_MODULE_M1  11      /** largeur d'un QRCode dans le mode M1  */
#define NB_MODULE_M2  13      /** largeur d'un QRCode dans le mode M2  */
#define NB_MODULE_M3  15      /** largeur d'un QRCode dans le mode M3  */
#define NB_MODULE_M4  17      /** largeur d'un QRCode dans le mode M4  */

#warning selectionner la taille des qrcode selon le mode choisi ci dessous
#define NB_MODULE NB_MODULE_M1 /// Taille d'un module ( A SELECTIONNER)

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
//int alphanum_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version);   // sujet 3b : Très difficile
//int    ascii_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version);   // sujet 3a : moyennement difficile

int data_string_to_binaryDS(const unsigned char data_string[24+1],  // sujets 3a,b,c (facultatif)  : aiguillage vers les fonctions xxx_to_binary_DS
                            unsigned char binaryDS[24*8],
                            unsigned short int version,
                            unsigned short int mode );

void binaryDS_to_packedbyteDS(const unsigned char binaryDS[24*8], unsigned char packedbyteDS[24],unsigned short int version, int longueurBinaryDS); // sujets 3a,b,c ! recompaction en bloc de 8 (sauf M1/M3 qui comportent 1 bloc de 4
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

int main(void)
{

    //test_unitaire_sujet0();
    //test_unitaire_sujet1();
    //test_unitaire_sujet2();
    test_unitaire_sujet3();
    test_unitaire_sujet4();

    return 0;
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet0(void)
///\brief exemple de test unitaire pour le sujet 0
///
void test_unitaire_sujet0(void)
{
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
    printf("\n Test unitaire 0 : MicroQRcode 100% BLANC \n");
    QRcode_to_console(MicroQRcode);

    // initialisation eu microQRcode (finder patterne + synchro pattern) + verification
    initialise_QRcode(MicroQRcode);

    printf("\n Test unitaire 0 : MicroQRcode initialisé ave ses finder et synchronous patterns \n");
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
void test_unitaire_sujet1(void)
{
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
void test_unitaire_sujet2(void)
{
    unsigned char MicroQRcode[NB_MODULE][NB_MODULE];  // le microQRcode sans la Quiet ZONE
    unsigned char MicroQRmask[NB_MODULE][NB_MODULE];  // le mask
    printf(" MicroQRcode M1 \n");
    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    QRcode_to_console(MicroQRcode);
    unsigned short int mode_microQRcode = M1_ ;    // choix du mode
    unsigned short int mask_number = 0;             // choix du n° de MASK

    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    efface_QRcode(MicroQRmask);
    genere_QRmask(MicroQRmask,0);
    xor_QRcode_QRmask(MicroQRcode,MicroQRmask);
    QRcode_to_pgm(MicroQRcode,"images/QRcode_Masque0.pgm");

    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    efface_QRcode(MicroQRmask);
    genere_QRmask(MicroQRmask,1);
    xor_QRcode_QRmask(MicroQRcode,MicroQRmask);
    QRcode_to_pgm(MicroQRcode,"images/QRcode_Masque1.pgm");

    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    efface_QRcode(MicroQRmask);
    genere_QRmask(MicroQRmask,2);
    xor_QRcode_QRmask(MicroQRcode,MicroQRmask);
    QRcode_to_pgm(MicroQRcode,"images/QRcode_Masque2.pgm");

    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    efface_QRcode(MicroQRmask);
    genere_QRmask(MicroQRmask,3);
    xor_QRcode_QRmask(MicroQRcode,MicroQRmask);
    QRcode_to_pgm(MicroQRcode,"images/QRcode_Masque3.pgm");

    int i, score, scoreMax = 0, masque;
    for(i = 0; i < 4; i++)
    {
        printf(" \n Masque %d \n",i);
        efface_QRcode(MicroQRcode);
        initialise_QRcode(MicroQRcode);
        efface_QRcode(MicroQRmask);
        genere_QRmask(MicroQRmask,mask_number);
        xor_QRcode_QRmask(MicroQRcode,MicroQRmask);
        printf(" \n Score micro QR avec masque %d \n",score_masquage_QRcode(MicroQRmask));
        QRcode_to_console(MicroQRcode);
        if(score_masquage_QRcode(MicroQRmask) > scoreMax)
        {
            scoreMax = score_masquage_QRcode(MicroQRmask);
            masque = i;
        }
        mask_number++;
    }
    printf("\n\n Score maximal pour ce qrcode : %d | Masque choisi : %d",scoreMax,masque);
    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    genere_QRmask(MicroQRmask,masque);
    xor_QRcode_QRmask(MicroQRcode,MicroQRmask);
    QRcode_to_console(MicroQRcode);
    QRcode_to_pgm(MicroQRcode,"images/QRcode_Sans_Donnees_Meilleur_Masque.pgm");
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet3(void)
///\brief tests unitaire pour le sujet 3
///
void test_unitaire_sujet3(void)
{
    int longueurChaineAChiffrer;
    unsigned char data_string[24+1]="0123456";  // un autre pour test de l'encodage numérique ( 2 triplets de chiffres)
    unsigned char binaryDS[24*8];
    unsigned char packedbyteDS[24];      //Flux binaire compacté en paquet de 8 bits (initalisé uniquement pour les tests)
    //unsigned short int mode_microQRcode = M1_ ;    // choix du mode (
    //unsigned short int mask_number = 2;             // choix du n° de MASK
    printf(" Chiffres entres: ");
    datastring_to_console(data_string);
    longueurChaineAChiffrer = numeric_to_binaryDS(data_string,binaryDS,0);
    binaryDS_to_packedbyteDS(binaryDS,packedbyteDS,0,longueurChaineAChiffrer);

    //printf("Affichage de la binary data string \n ");
    //binaryDS_to_console(binaryDS);
}

///////////////////////////////////////////////////////////
///\fn void test_unitaire_sujet4(void)
///\brief tests unitaire pour le sujet 4
///
void test_unitaire_sujet4(void)
{
    unsigned char MicroQRcode[NB_MODULE][NB_MODULE];  // le microQRcode sans la Quiet ZONE
    unsigned char packedbyteDS[24];
    unsigned short int mode_microQRcode = M1_ ;    // choix du mode (



    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    printf(" Bloc 1 de 8 bits (UP):");
    put_byte_in_blocks(MicroQRcode,224,10,10,1);
    QRcode_to_console(MicroQRcode);

    printf(" Bloc 2 de 8 bits (UP):");
    put_byte_in_blocks(MicroQRcode,98,6,10,1);
    QRcode_to_console(MicroQRcode);

    printf(" Bloc 3 de 4 bits (US):");
    put_byte_in_blocks(MicroQRcode,11,2,10,5);
    QRcode_to_console(MicroQRcode);

    printf(" Bloc 4 de 8 bits (UL):");
    put_byte_in_blocks(MicroQRcode,44,9,8,3);
    QRcode_to_console(MicroQRcode);

    printf(" Bloc 5 de 8 bits (UL):");
    put_byte_in_blocks(MicroQRcode,0,9,4,3);
    QRcode_to_console(MicroQRcode);

    printf(" Test d'Ajoute_dataM1_QRcode :");
    efface_QRcode(MicroQRcode);
    initialise_QRcode(MicroQRcode);
    ajoute_dataM1_QRcode(packedbyteDS,MicroQRcode);
    QRcode_to_console(MicroQRcode);
}
// FIN DES TESTS UNITAIRES
//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/// \fn void efface_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
/// \brief Sujet 0 (commun tous sujets 1,2,3,4) :  efface entierement un qrcode (ou un qrmask) et positione tous ses modules a 255 (BLANC)
/// \param[out] qrcode[][]   qrcode (ou qrmask) a effacer
void efface_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    int i,j;
    for(i = 0; i < NB_MODULE; i++) // parcours des lignes
    {
        for(j = 0; j < NB_MODULE; j++) // parcours des colonnes
        {
            qrcode[i][j] = BLANC; // Chaque case est blanche sur l'image et un point sur la console
        }
    }
}

///////////////////////////////////////////////////////////////////
/// \fn void initialise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE]){
/// \brief Sujet 0 (commun tous sujets 1,2,3,4) : initialise les pattern  obligatoires d'un QRCODE
///             finder pattern de 7x7 module en haut a gauche
///            + synchonous pattern coté haut et coté gauche
///             le qrcode doit etre préalablement effacé (100% blanc)
/// \param[in,out] qrcode[][]    qrcode à initialiser

void initialise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    int i,j;
    for(i = 0; i < NB_MODULE; i++)
    {
        for(j = 0; j < NB_MODULE; j++)
        {
            if((i == 0 || i == 6) && j < 7) // Tracé du côté Nord et Sud du carré
            {
                qrcode[i][j] = NOIR;
            }
            if(i == 0 && j >= 7 && j % 2 == 0) // Tracé du timing pattern sur la première ligne
            {
                qrcode[i][j] = NOIR;
            }
            if((j == 0 || j == 6) && i < 7) // Tracé des côtés Ouest et Est du carré
            {
                qrcode[i][j] = NOIR;
            }
            if(j == 0 && i >= 7 && i % 2 == 0) // Tracé du timing pattern sur la première colonne
            {
                qrcode[i][j] = NOIR;
            }
            if(i >= 2 && i <= 4 && j >= 2 && j <= 4) // Tracé du carré central du finding pattern
            {
                qrcode[i][j] = NOIR;
            }
        }
    }
    /*qrcode[3][10] = NOIR;
    qrcode[6][16] = NOIR;
    qrcode[10][4] = NOIR;
    qrcode[15][7] = NOIR;*/
}

///////////////////////////////////////////////////////////////////////////
/// \fn genere_QRmask(unsigned char qrmask[NB_MODULE][NB_MODULE],int no_masque)
/// \brief Sujet2 : Generation des qrmask pour masquage final une fois le qrcode complet
/// \param[out] qrmask[][] : le qrmak a remplir
/// \param[in] no_masque  : n° de masque de 0 a 3 (cf masque dans ISO18004/2015 p52 et 54)

void genere_QRmask(unsigned char qrmask[NB_MODULE][NB_MODULE],int no_masque)
{
    int i,j;

    switch(no_masque)
    {
        case 0:
            for(i = 0; i < 11; i++)
            {
                for(j = 0; j < 11; j++)
                {
                    if(((i > 0 && i <= 8) && j > 8) || (i > 8 && j > 0 ))
                    {
                        if(i % 2 == 0)
                            qrmask[i][j] = NOIR;
                        else
                            qrmask[i][j] = BLANC;
                    }
                }
            }
            break;
        case 1:
            for(i = 0; i < 11; i++)
            {
                for(j = 0; j < 11; j++)
                {
                    if(((i > 0 && i <=8) && j > 8) || (i > 8 && j > 0 ))
                    {
                        if(((i / 2) + (j / 3)) % 2 == 0)
                            qrmask[i][j] = NOIR;
                        else
                            qrmask[i][j] = BLANC;
                    }
                }
            }

            break;
        case 2:
            for(i = 0; i < 11; i++)
            {
                for(j = 0; j < 11; j++)
                {
                    if(((i > 0 && i <= 8) && j > 8) || (i > 8 && j > 0 ))
                    {
                        if((((i * j) % 2) + ((i * j) % 3)) % 2 == 0)
                            qrmask[i][j] = NOIR;
                        else
                            qrmask[i][j] = BLANC;
                    }
                }
            }
            break;
        case 3:
            for(i = 0; i < 11; i++)
            {
                for(j = 0; j < 11; j++)
                {
                    if(((i > 0 && i <= 8) && j > 8) || (i > 8 && j > 0 ))
                    {
                        if((((i + j) % 2) + ((i * j) % 3)) % 2 == 0)
                            qrmask[i][j] = NOIR;
                        else
                            qrmask[i][j] = BLANC;
                    }
                }
            }
            break;
    }





}

///////////////////////////////////////////////////////////////////////
/// void Black_And_Whitise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief  Code C fourni : cette fonction transforme tout ce qui n'est pas BLANC  (=255) en NOIR (=0) ,
///         Pour le debug, on a utilisé les majuscules F, S, V... pour reperer les zone finder, synchro, version
/// \param[in,out] qrcode[][] : le qrcode a mettre en noir et blanc
void Black_And_Whitise_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    int i,j;
    for(i=0; i<NB_MODULE; i++)
    {
        for(j=0; j<NB_MODULE; j++)
        {
            if(qrcode[i][j] < BLANC)
            {
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

void xor_QRcode_QRmask(unsigned char qrcode[NB_MODULE][NB_MODULE],const unsigned char qrmask[NB_MODULE][NB_MODULE])
{
    int i,j;
    for(i = 0; i < 11; i++)
            {
                for(j = 0; j < 11; j++)
                {
                    if(((i > 0 && i <= 7) && j > 8) || (i > 8 && j > 0 ))
                    {
                        if(qrcode[i][j] == NOIR && qrmask[i][j] == NOIR)
                            qrcode[i][j] = BLANC;
                        else if(qrcode[i][j] == NOIR && qrmask[i][j] == BLANC)
                            qrcode[i][j] = NOIR;
                        else if(qrcode[i][j] == BLANC && qrmask[i][j] == NOIR)
                            qrcode[i][j] = NOIR;
                        else
                            qrcode[i][j] = BLANC;
                    }
                }
            }
}
////////////////////////////////////////////////////////////////
/// \fn int score_masquage_QRcode(const unsigned char qrcode[NB_MODULE][NB_MODULE]){
/// \brief  Sujet 2 (facultatif)  : calcul du score apres masquage (ISO18004 page papier 53/54)
/// le principe est de tester les 4 masques definis pour les microQRcode et de choisir celui qui maximise ce score.
/// dans un 1er temps on n'optimisera pas ce masquage, mais on appliquera un masque fixe au choix du programmeur
///  toutefois la fonction de calcul du score est ci dessous
/// \param[in] qrcode[][] : le qrcode dont on doit calculer le score
/// \return Le score calculé

int score_masquage_QRcode(const unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    int som_1=0, som_2=0;
    int score=0;
    int i, j;
    for(i = 0; i < NB_MODULE; i++)
    {
        for(j = 0; j < NB_MODULE; j++)
        {
            if((i > 0) && (j == (NB_MODULE - 1)) && (qrcode[i][j] == NOIR))
                som_1++;
            if((i == NB_MODULE -1) && (j > 0) && (qrcode[i][j] == NOIR))
                som_2++;
        }
    }
    if(som_1 <= som_2)
        score = som_1 * 16 + som_2;
    else
        score = som_2 * 16 + som_1;

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
unsigned short int  encode_version(int type, int no_masque)
{
    unsigned short int header15bits  = 0;
    unsigned char header[15];
    int i, puissance2 = 16384;
    if(type == M1_)
    {
        header[0] = ((type & 0x04) != 0x00);
        header[1] = ((type & 0x02) != 0x00);
        header[2] = ((type & 0x01) != 0x00);
        header[3] = ((no_masque & 0x02) != 0x00);
        header[4] = ((no_masque & 0x01) != 0x00);

    }
    switch(no_masque)
    {
        case 0:
            header[5] = 0;
            header[6] = 0;
            header[7] = 0;
            header[8] = 0;
            header[9] = 0;
            header[10] = 0;
            header[11] = 0;
            header[12] = 0;
            header[13] = 0;
            header[14] = 0;
            break;

        case 1:
            header[5] = 0;
            header[6] = 1;
            header[7] = 0;
            header[8] = 0;
            header[9] = 1;
            header[10] = 1;
            header[11] = 0;
            header[12] = 1;
            header[13] = 1;
            header[14] = 1;
            break;

        case 2:
            header[5] = 1;
            header[6] = 0;
            header[7] = 0;
            header[8] = 1;
            header[9] = 1;
            header[10] = 0;
            header[11] = 1;
            header[12] = 1;
            header[13] = 1;
            header[14] = 0;
            break;

        case 3:
            header[5] = 1;
            header[6] = 1;
            header[7] = 0;
            header[8] = 1;
            header[9] = 0;
            header[10] = 1;
            header[11] = 1;
            header[12] = 0;
            header[13] = 0;
            header[14] = 1;
            break;
    }
    printf(" %s",header);
    for(i = 0; i < 15; i++)
    {
        header15bits += header[i] * puissance2;
        puissance2 /= 2;
    }
    return header15bits;
}

////////////////////////////////////////////////////
/// \fn void ajoute_version_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned short int header15bits){ajoute l'information de version sur le QRCODE
/// \param[in,out]  qrcode[][]  LE qrcode !
/// \param[in] : header15bits   entete a ecrire (version + n°mack + 10 bits BCH) précalculées
/// \brief Sujet 1 :  ajoute les 15 bits de version (deja calculés) dans la zone de 15 bits reservées qui entourent le FINDER PATTERN

void ajoute_version_QRcode(unsigned char qrcode[NB_MODULE][NB_MODULE],unsigned short int header15bits)
{
//A COMPLETER
}

//////////////////////////////////////////////////////Ma partie///////////////////////////////////////////////////////
/// \fn int numeric_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version)
/// \brief Sujet 3c (version M1) : Encode une datastring de type numérique  en chaine binaire (dont entete longuer/mode)
///      OBLIGATOIRE : longueur de chaine multiple de 3, facultatif (toute longueur)
/// \param[in]  data_string[] Chaine de caracteres a encoder dans le QRcode
/// \param[out] binaryDS[]    Chaine binaire encodée
/// \param[in]  version parmi M1_, M2_L, M2_M, M3....
///         toutes les versions (M1,M2,M3,M4) supportent le mode numérique
/// \return Retourne la longueur du datastream ou -1 si mode non supporté


int numeric_to_binaryDS(const unsigned char data_string[24+1], unsigned char binaryDS[24*8], unsigned short int version)
{
    printf("\n\n***********************Chiffrement des donnees sur 10 bits***********************\n\n");
    int nb_chiffres = 0;        // nombre de chiffres dans la chaine numérique a encoder

    printf(" NB chiffres %d |",strlen(data_string));
    nb_chiffres = (float)strlen(data_string); // nb_chiffres est mis en float pour pouvoir le tronquer juste après

    int nbTriplets = (int) ceil(nb_chiffres / 3.0); // On prend la valeur supérieure de nb_chiffres / 3.0 et on la transforme en valeur entière

    printf(" NB triplets %d |",nbTriplets);

    char stockTriplets[nbTriplets][4]; // Création d'un tableau à 2 dimensions : lignes = nbTriplets ; colonnes = 4 (3 chiffres + \0)

/////////////////////////////////Stockage des triplets dans un tableau 2D/////////////////////////////////
    int i = 0,ligne, colonne, nbChiffresTriplet = 0, cptChiffresTriplet;

    for(ligne = 0; ligne < nbTriplets; ligne++)
    {
        if((ligne == (nbTriplets - 1)) && (nb_chiffres % 3 != 0))
        {
            cptChiffresTriplet = nb_chiffres % 3;
        }
        else
        {
            cptChiffresTriplet = 3;
        }
        for(colonne = 0; colonne < cptChiffresTriplet; colonne++)
        {
            stockTriplets[ligne][colonne] = data_string[i];
            i++;
        }
        stockTriplets[ligne][cptChiffresTriplet] = '\0';
    }
///////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////Transformation en binaire puis envoie dans le tableau binaryDS du count indicator (3 bits pour M1)/////////////////////////////////
    // Le maximum de valeurs est bien de 7 chiffres (3 bits pou M1)
    binaryDS[0] = ((nb_chiffres & 0x04) != 0x00); // On extrait et on place dans les trois premiers bits de binaryDS les trois bits du count indicator
    binaryDS[1] = ((nb_chiffres & 0x02) != 0x00);
    binaryDS[2] = ((nb_chiffres & 0x01) != 0x00);
    printf(" Count Indicator: ");
    for(int z = 0; z < 3; z++)
        printf(" %d", binaryDS[z]);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////Transformation en binaire puis envoie dans le tableau binaryDS des triplets(10,7,4 bits)/////////////////////////////////
    int j, k, decatlet, nbChiffresDernierTriplet, maximum, cpt = 3; // cpt commence à trois car les trois premiers bits de binaryDS contiennent le count indicator
    nbChiffresDernierTriplet = nb_chiffres % 3;
    unsigned int mask;
    for(j = 0; j < nbTriplets; j++)
    {
        printf("\n\n Triplet(mode decimal): %s |",stockTriplets[j]);
        decatlet = atoi(stockTriplets[j]);
        printf(" Triplet(mode hexadecimal): 0x%x \n",decatlet);
        mask = 0b1000000000; // masque initial pour extraire les 10,7,4 bits de mon nombre à 3 chiffres
        maximum = 10;
        if((j == nbTriplets - 1) && (nbTriplets > 1))
        {
            if(nbChiffresDernierTriplet == 1)
            {
                maximum = 4;
                mask = 0b0000001000;
            }
            if(nbChiffresDernierTriplet == 2)
            {
                maximum = 7;
                mask = 0b0001000000;
            }
        }
        //printf(" Masque : %d",mask);

        for(int i = 0; i < maximum; i++)
        {
            if((mask & decatlet) == 0x00)  // le bit extrait est nul
            {
                binaryDS[cpt] = 0;
            }
            else
            {
                binaryDS[cpt] = 1;
            }

            printf("\n Ou se trouve le 1 du mask: (decimal): %d | (hexadecima): 0x%x", mask,mask);
            mask = mask >> 1;
            cpt++;
            //printf("\nICI");
        }
    }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////Ajout du Terminator (3 0 pour un M1) et du 255 en fin de chaîne/////////////////////////////////
    int indexFin;
    for(indexFin = cpt; indexFin < cpt + 4; indexFin++)
        binaryDS[indexFin] = 0;
    cpt += 3;
    binaryDS[cpt] = 255;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    printf("\n\n CPT(longueur de la chaine binaryDS) %d ",cpt);
    printf("\n\n Fonction binaryDS_to_console: ");
    binaryDS_to_console(binaryDS);
    printf("\n\n Affichage de binaryDS: ");
    for(k = 0; k < cpt; k++)
        printf(" %d", binaryDS[k]);

    return cpt-1;
}

/*
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
                                unsigned short int mode )
    {
        // TODO : controle de cohérence mode/version et aiguillage vers l'eune des 3 fonctions


        return 0;
    }
*/
// //////////////////////////////Ma partie//////////////////////////////////////////////////////////////////
/// \fn  void binaryDS_to_packedbyteDS(const unsigned char binaryDS[], unsigned char packedbyteDS[],unsigned short int version)
/// \brief Tous sujets 3 (3a,3b,3c) :Pack un binary DS en packed byte (gere le cas particulier du bloc de 4bits M2/M3L/M3M
/// \param[in]   binaryDS[], la chaine binaire a compacter
/// \param[out]  packedbyteDS[24] : le tableau du flux binaire a ecrire compacté en octets (seuls les 5 premiers sont ecrits
/// \param[in]   version parmi M1_, M2_L .... M3_Q
///              ATTENTION : en M1 et M3, le bloc specifique de 4 bit est calé sur le MSB (bit7 a 4) de l'octet


void binaryDS_to_packedbyteDS(const unsigned char binaryDS[24*8], unsigned char packedbyteDS[24],unsigned short int version, int longueurBinaryDS)
{
    unsigned char stockOctet[9];
    unsigned char stockQuartet[5];
    int longueurChaineBinaire;
    longueurChaineBinaire = longueurBinaryDS;
    printf("\n\n***********************Compactage des donnees sur 8 bits***********************\n\n");
    printf(" Longueur de la chaine binaire: %d \n",longueurChaineBinaire);

    int i, j, indexBinaryDS = 0, indexOctetQuartet, nbCaracteresAParcourir;
    int puissance2, somme, valeur;
    for(i = 0; i < 5; i++)
    {
        if(i == 2)
        {
            nbCaracteresAParcourir = 4;
            puissance2 = 8;
        }
        else
        {
            nbCaracteresAParcourir = 8;
            puissance2 = 128;
        }
        indexOctetQuartet = 0;
        somme = 0;
        for(j = nbCaracteresAParcourir; j > 0; j--)
        {
            if(indexBinaryDS >= longueurChaineBinaire)
                valeur = 0;
            else
            {
                valeur = binaryDS[indexBinaryDS] * puissance2;
            }
            somme += valeur;
            printf(" Somme : %d | %d | %d \n",somme,binaryDS[indexBinaryDS],puissance2);
            puissance2 /= 2;
            indexBinaryDS++;
        }
        packedbyteDS[i] = somme;
        printf(" Packed: %d \n", somme);
    }
}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM1_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief Sujet 4 version M1 : Ajoute les données (packed) dans un microQRcode M1
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 5 premiers sont ecrits
/// \param[in,out] qrcode[][]  : le qrcode
void ajoute_dataM1_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    int i;
    for(i = 0; i < 5; i++)
    {
        if(i < 2)
            put_byte_in_blocks(qrcode,packedbyteDS[i],10 - i * 4,10,UP);
        else if(i == 2)
            put_byte_in_blocks(qrcode,packedbyteDS[i],10 - i * 4,10,US);
        else
            put_byte_in_blocks(qrcode,packedbyteDS[i],9,8 - (i-3) * 4,UL);
    }

}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM2_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief Sujet 4 version M2 : Ajoute les données (packed) dans un microQRcode M2
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 10 premiers sont ecrits
/// \param[in,out] qrcode[][]  : le qrcode

void ajoute_dataM2_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    //int i;
    // A completer
}
//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM3L_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief HORS SUJET  M3L : Ajoute les données (packed)  dans un microQRcode M3L
/// \param[in]      packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 17 premiers sont ecrits
/// \param[in,out]  qrcode[][]  : le qrcode

void ajoute_dataM3L_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    //int i;
    // NON TRAITE
}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM3M_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief HORS SUJET  M3M : ajoute les données (packed) dans un microQRcode M3M
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets (seuls les 17 premiers sont ecrits
/// \param[in,out] qrcode[][]  : le qrcode !

void ajoute_dataM3M_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    //int i;
    // NON TRAITE

}

//////////////////////////////////////////////////////////////////////
/// \fn void ajoute_dataM4_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
/// \brief Sujet 4 (version M4) : Ajoute les données (packed) dans un microQRcode M4
/// \param[in]     packedbyteDS[] : le tableau du flux binaire a ecrire compacté en octets
/// \param[in,out] qrcode[][] : le qrcode

void ajoute_dataM4_QRcode(const unsigned char packedbyteDS[24],unsigned char qrcode[NB_MODULE][NB_MODULE])
{
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
                        unsigned char lig, unsigned char col, unsigned char direction)
{
    byte = (int) byte;
    char byteBinaire[8];
    int puissance2 = 128, valeurDecimale = 0,k;

    for(k = 0; k < 8; k++)
    {

        if((valeurDecimale + puissance2) <= byte)
        {
            valeurDecimale += puissance2;
            byteBinaire[k] = 1;
            puissance2 /= 2;
            printf(" \n valeur decimale: %d | %d",valeurDecimale,puissance2);
        }
        else
        {
            byteBinaire[k] = 0;
            puissance2 /= 2;
        }
    }
    for(int index = 0; index < 8; index++)
    {
        printf(" %d",byteBinaire[index]);
    }

    printf(" %s",byteBinaire);
    int i,j,cptByteBinaire = 0;
    switch(direction)
    {
        case 1:
            for(i = lig; i >= (lig - 3); i--)
            {
                for(j = col; j >= (col - 1); j--)
                {
                    if(byteBinaire[cptByteBinaire] == 1)
                        qrcode[i][j] = NOIR;
                    else
                        qrcode[i][j] = BLANC;
                    cptByteBinaire++;
                }
            }
            break;

        case 3:
            for(i = lig; i <= (lig + 1); i++)
            {
                cptByteBinaire = 0;
                for(j = col; j >= (col - 3); j--)
                {
                    if((j % 2 == 0) && (byteBinaire[cptByteBinaire] == 1))
                    {
                        qrcode[i][j] = NOIR;
                        cptByteBinaire += 2;
                    }
                    else if((j % 2 == 0) && (byteBinaire[cptByteBinaire] == 0))
                    {
                        qrcode[i][j] = BLANC;
                        cptByteBinaire += 2;
                    }
                    else if((j % 2 == 1) && (byteBinaire[cptByteBinaire] == 1))
                    {
                        qrcode[i][j] = NOIR;
                        cptByteBinaire = cptByteBinaire * 2 + 1;
                    }

                    else if((j % 2 == 1) && (byteBinaire[cptByteBinaire] == 0))
                    {
                        qrcode[i][j] = BLANC;
                        cptByteBinaire = cptByteBinaire * 2 + 1;
                    }


                }
            }
            break;

        case 5:
            for(i = lig; i >= (lig - 1); i--)
            {
                for(j = col; j >= (col - 1); j--)
                {
                    if(byteBinaire[cptByteBinaire] == 1)
                        qrcode[i][j] = NOIR;
                    else
                        qrcode[i][j] = BLANC;
                    cptByteBinaire++;
                }
            }
            break;
    }
}

/////////////////////////////////////////////////////////////////////////
/// \fn void QRcode_to_console(const unsigned char qrcode[NB_MODULE][NB_MODULE]){
///  \brief  Code C fourni  : Fonction d'affichage basique du microQRcode sur la console
/// affiche un microQRcode sur la console, . pour les blanc, * pour les noirs, F,V, S pour reperer les differentes
/// zone lors du debug.
/// \param[in] qrcode : le QRcode a afficher sur la console !
void QRcode_to_console(const unsigned char qrcode[NB_MODULE][NB_MODULE])
{
    int i,j;
    putchar('\n');
    for(i=0; i<NB_MODULE; i++)
    {
        for(j=0; j<NB_MODULE; j++)
        {
            switch(qrcode[i][j])                                      // on ajoute 1 espace entre module pour que ca ressemble a un carré
            {
            case BLANC :
                putchar('.');
                putchar(' ');
                break;       // des . pour le blanc /
            case NOIR  :
                putchar('*');
                putchar(' ');
                break;       // des * pour les noirs
            default    :
                if((qrcode[i][j] <=126) && (qrcode[i][j]>= 32) )  // on affiche les carectere "affichable"
                {
                    putchar(qrcode[i][j]);
                    putchar(' ');
                }
                else
                {
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
#define PIX_BY_MODULE 16
/////////////////////////////////////////////////////////////////////////
/// \fn int QRcode_to_pgm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename)
/// \brief Code C fourni : Fonction d'export en PGM (portable Gray map); format d'image lisible avec xniew
/// \param[in] qrcode[][] le QRcode !
/// \param[out] *filename     le nom du fichier image ou enregistre le qr code (avec extension .pgm)

// la constante précédente PIX_BY_MODULE, definit la taille en pixels d'un module dans le fichier PGM
// exemple si PIX_BY_MODULE =8, chaque module =1bit)= deviendra 8 x 8 pixels dans l'image
// le qrcode passé doit etre codé en niveau de gris (0 pour le noir, 255 pour le blanc)..

int QRcode_to_pgm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename)
{
    FILE *fd;
    int i,j,ii,jj;

//Test sur l'ouverture du fichier
    if(!(fd = fopen(filename,"w+")) )
    {
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
    for(i=0; i<NB_MODULE; i++)
    {
        for(ii=0; ii<PIX_BY_MODULE; ii++) // on recopie plusieurs fois le module dans 8 pixels*8pixels
        {
            for(j=0; j<NB_MODULE; j++)
            {
                for(jj=0; jj<PIX_BY_MODULE; jj++)
                {
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
int QRcode_to_ppm(const unsigned char qrcode[NB_MODULE][NB_MODULE], char *filename, unsigned long int color)
{
    FILE *fd;
    int i,j,ii,jj;
    unsigned char red_level,green_level,blue_level; // RGB pour la couleur des pixels
    int w = NB_MODULE*PIX_BY_MODULE ; // surechantillonnage
    int h = NB_MODULE*PIX_BY_MODULE ; // surechantillonnage
    red_level   = (color & 0x00FF0000)>>16;     // extraction des couleurs primaires R,G,B
    green_level = (color & 0x0000FF00)>>8;      // pour ecriture dans le PPM
    blue_level  = (color & 0x000000FF);

//Test sur l'ouverture du fichier
    if(!(fd = fopen(filename,"w+")) )
    {
        fprintf(stderr,"QRcode2ppm, erreur de cr�ation du fichier %s\n",filename);
        exit(-1);
    };

// Entete d'un fichier ppm  P6 = Magick number....
    fprintf(fd,"P6\n%d %d\n255 ",w,h);  // entete ppm P6 : largeur, hauteur, nb niveaux sur  canaux RGB

// RGBRGBRGBRGB a la suite jusqu'a la fin en partant en haut a gauche de l'image
    for(i=0; i<NB_MODULE; i++)
    {
        for(ii=0; ii<PIX_BY_MODULE; ii++) // on recopie plusieurs fois le module dans 8 pixels*8pixels
        {
            for(j=0; j<NB_MODULE; j++)
            {
                for(jj=0; jj<PIX_BY_MODULE; jj++) // repetition par colonne
                {
                    if(qrcode[i][j] == BLANC)
                    {
                        fprintf(fd,"%c%c%c",255,255,255);  //
                    }
                    else
                    {
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
void datastring_to_console(const unsigned char datastring[])
{
    int index = 0;
    do
    {
        putchar(datastring[index++]);
    }
    while((index < 24) && (datastring[index] != '\0'));  // \0 en fin de chaine (sauf oubli)
    putchar('\n');
}

//////////////////////////////////////////////////////////////
/// \fn  void binaryDS_to_console(const unsigned char binaryDS[])
/// \brief Code C fourni : affiche un flux binaire sur la console (32 bits/ligne)
/// \param[in] binaryDS[] : la chaine binaire (255 = 0xFF = finde chaine binaire
/// rappel, un binary string est un tableau de 0 et 1 . la fin est marquée par la valeur 255
void binaryDS_to_console(const unsigned char binaryDS[])
{
    int index = 0;

    do
    {
        if(index%32 == 0) putchar('\n');   // 4 octets par ligne
        if(index%8 == 0)  printf(" 0b");   // 0b devant chaque
        if(binaryDS[index] == 0)
        {
            putchar('0');
        }
        if(binaryDS[index] == 1)
        {
            putchar('1');
        }
        index++;
    }
    while((index < 24*8) && (binaryDS[index] < 255));
    putchar('\n');
}

////////////////////////////////////////
/// \fn  void packedbyte_to_console(const unsigned char packedbyteDS[])
/// \brief Code C fourni : Affiche un packedbyte Data stream sur la consoel
/// \param[in] packedbyteDS[] : le tableau d'octet a ecrire

void packedbyte_to_console(const unsigned char packedbyteDS[])
{
    int index = 0 ;
    putchar('\n');
    do
    {
        printf("0x%x ",packedbyteDS[index++]);
        if(index%8 == 0) putchar('\n');
    }
    while((index<24) && (packedbyteDS[index] < 255));
    //#warning 'bug a corriger : si un packedbyte = 255 .... fin d'affichage'
    putchar('\n');
}

