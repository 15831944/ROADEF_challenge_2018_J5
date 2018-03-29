#pragma once

#include <string>
#include <vector>
#include <stack>

#include "glass_node.h"
#include "glass_stack.h"
#include "glass_item.h"
#include "glass_item.h"
#include "glass_plate.h"

#include "global_var.h"
#include "parser.h"


class Algorithm{
    
    public:

        // Le constructeur prend en entrée le nom de l'instance courante à étudier
        Algorithm(std::string instance);

        // renvoie la valeur du score correspondant à la déf. de la roadef 
        unsigned int get_score_roadef(); 

        // s'assure que toutes les contraintes sont vérifiées en utilisant le code fourni
        // a priori long mais pas besoin de l'utiliser en fonctionnement normal
        bool checker_complete();

        // Gestion du temps de calcul
        void init_cpu_time(); // Initialise un compteur
        int get_current_time(); // Renvoie la valeur actuelle du compteur

        // Gestion d'un fichier de log (résultats et paramètres pour faire des stats dessus)
        // Enregistre dans un fichier de log les résultats obtenus ainsi que des paramètres
        void log_results();

        // Ecrit dans des fichiers de manière cohérente la solution sol_items
        void export_solution();

        // ---------------------------------------------
        //            GLOUTON

         // met à jour les attributs de Algorithm avec un algorithme glouton
        void glouton(method m = basic);

        // Renvoie la meilleure position en bas à gauche pour une pièce (variables par références x et y)
        // et une orientation (rotated == true si on regarde la version pivotée de pi/2)
        // les contraintes. False si aucune solution possible
        bool position_defects(const GlassNode& node, const GlassItem& item, bool rotated, int& x, int& y);
        bool intersect(const int& x, const int& y, const int& w, const int& h, const GlassDefect& defect);


        // Calcule un score selon un critère (paramètre avec l'enumerate method - voir global_var.h)
        float score(const GlassNode& node, const GlassItem& item, bool rotated, const int& x, const int& y,  method m = basic);
        
        // Renvoie la référence vers la pièce à découper et sa position en vue de la découpe
        // et false si aucune pièce n'est découpable dans le node
        bool decision(const GlassNode& node, GlassItem& item, bool& rotated, int& x, int& y, method n = basic);

        // Coupe la pièce la plus appropriée selon la décision
        void cut(const GlassItem& item, GlassNode& node, const bool& rotated, const int& x, const int& y, stack<GlassNode> nodes_to_visit);


        std::vector<GlassStack> Getstacks() const { return stacks; }
        std::vector<GlassPlate> Getplates() const { return plates; }
        std::vector<GlassItem> Getitems() const { return items; }
        std::vector<GlassNode> Getsol() const { return sol; }

    private:
        // Paramètres du problème
        std::string name_instance;
        std::vector<GlassPlate> plates;
        std::vector<GlassStack> stacks;
        std::vector<GlassItem> items;

        // Solution 
        std::vector<GlassNode> sol; // Cutting pattern
};
