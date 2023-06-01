#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cbor.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <tinyxml2.h>
#include <thread>
#include <mutex>
#include <condition_variable>

std::mutex sun_position_mutex;
std::condition_variable data_received;

double sun_x;
double sun_y;


using namespace std;
using namespace tinyxml2;

static void do_drawing(cairo_t *);
static void do_drawing_svg(cairo_t *);

RsvgHandle *svg_handle;
RsvgRectangle viewport;

XMLDocument svg_data;

GtkWidget *window;
GtkWidget *darea;

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{      
  do_drawing_svg(cr);

  return FALSE;
}

static void do_drawing_svg(cairo_t * cr)
{
  rsvg_handle_render_document (svg_handle, cr, &viewport, NULL);
}

XMLElement* findElementSvg(XMLElement* element, const char* attribute) {
    XMLElement* elementParent;
    for (XMLElement* childAttribute = element->FirstChildElement(); childAttribute != NULL; childAttribute = childAttribute->NextSiblingElement()) {
        if (strcmp(childAttribute->Value(), "circle") == 0) {
            return childAttribute;
            const char* target_cx = childAttribute->Attribute("cx");
            const char* target_cy = childAttribute->Attribute("cy");
            if (strcmp(childAttribute->FirstChildElement("driven")->Attribute("by"), attribute) == 0) {
                return childAttribute->FirstChildElement("driven")->Parent()->ToElement();
            }
        } else {
            elementParent = findElementSvg(childAttribute, attribute);
            if (elementParent != NULL) {
                return elementParent;
            }
        }
    }
    return nullptr;
}

void read_initial_sun_position(XMLDocument& svg_data, double &initial_sun_x, double &initial_sun_y) {

    XMLElement *root = svg_data.FirstChildElement("svg");

    XMLElement *soleil_x = findElementSvg(root, "sun_x");
    if (soleil_x) {
        initial_sun_x = atof(soleil_x->Attribute("cx"));
    }

    XMLElement *soleil_y = findElementSvg(root, "sun_y");
    if (soleil_y) {
        initial_sun_y = atof(soleil_y->Attribute("cy"));
    }
}

void update_sun_position(XMLElement* element, const char* id, double x, double y) {
    XMLElement *circle_element = findElementSvg(element, id);
    if(circle_element){ 
        circle_element->SetAttribute("cx", x);
        circle_element->SetAttribute("cy", y);
    }
}

void update_svg(XMLDocument& svg_data) {

    // Element Contenant la balise svg du fichier 
    XMLElement *svg_sun = svg_data.FirstChildElement("svg");
    
    // On modifie les attributs cx et cy de la position du soleil grace au driven by sun_x et sun_y
    update_sun_position(svg_sun, "soleil", sun_x,sun_y);
    
    XMLPrinter printer;
    svg_data.Print(&printer);
    svg_handle = rsvg_handle_new_from_data ((const unsigned char*) printer.CStr(), printer.CStrSize(), NULL);
    rsvg_handle_get_geometry_for_element  (svg_handle, NULL, &viewport, NULL, NULL);
}

void update_image(XMLDocument& svg_data, GtkWidget *darea) {
    
    while (true) {
        
        std::unique_lock<std::mutex> lock(sun_position_mutex);
        data_received.wait(lock);
        update_svg(svg_data);
        gtk_widget_queue_draw(darea);
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}


void receive_data(int sockfd) {
    const int MAX_BUFFER_SIZE = 1024;
    unsigned char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);

    while (true) {
        int n = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&clientaddr, &clientaddrlen);

        if (n < 0) {
            std::cerr << "Erreur lors de la réception des données." << std::endl;
        } else {
            std::cout << "Paquet reçu de " << inet_ntoa(clientaddr.sin_addr) << ":" << ntohs(clientaddr.sin_port) << std::endl;


            struct cbor_load_result result;
            cbor_item_t *cbor_root = cbor_load(buffer, n, &result);
            
            if (!cbor_isa_map(cbor_root)) {
                std::cerr << "Expected a CBOR map." << std::endl;
                cbor_decref(&cbor_root);
            
            }
            
            cbor_pair *pairs;
            pairs = cbor_map_handle(cbor_root);

            double delta_x = 0.0;
            double delta_y = 0.0;

            for (size_t i = 0; i < cbor_map_size(cbor_root); i++) {
                cbor_item_t *key_item = pairs[i].key;
                cbor_item_t *val_item = pairs[i].value;
                unsigned char* key_str = (unsigned char*) cbor_string_handle(key_item);
                float val_float = cbor_float_get_float8(val_item);
                if (i == 0) {
                    delta_x = val_float;
                }
                else if (i == 1) {
                    delta_y = val_float;
                }
                std::cout << *key_str << " : " << val_float << std::endl;

            }
            
            cbor_decref(&cbor_root);
            {
                std::lock_guard<std::mutex> lock(sun_position_mutex);
                sun_x = delta_x;
                sun_y = delta_y;
            }
            data_received.notify_one();
        }
    }
}

int main(int argc, char *argv[]) {
    std::cout << "start" << std::endl;
    const int PORT = 6789;

    // Creation d'une socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Erreur lors de la création de la socket." << std::endl;
        exit (1);
    }

    // La structure sockaddr_in definie l'adresse et le port de la socket
    struct sockaddr_in servaddr;

    // Initialiser la structure à 0 avec memset
    memset(&servaddr, 0, sizeof(servaddr));
    
    // Pour IPv4
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(6789);


    // On lie une socket à une adresse et un numéro de port spécifiques.
    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    std::cout << "port = " << ntohs(servaddr.sin_port) << std::endl;
    
    gtk_init(&argc, &argv); 

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    darea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), darea);


    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);

    // On load le fichier maison.svg
    svg_data.LoadFile("maison.svg");

    double initial_sun_x = 0;
    double initial_sun_y = 0;
    read_initial_sun_position(svg_data, initial_sun_x, initial_sun_y);
    {
        std::lock_guard<std::mutex> lock(sun_position_mutex);
        sun_x = initial_sun_x;
        sun_y = initial_sun_y;
    }
    update_svg(svg_data);
    std::thread receive_data_thread(receive_data, sockfd);
    std::thread update_image_thread(update_image, std::ref(svg_data), darea);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL); 
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window),200, 200); 
    gtk_window_set_title(GTK_WINDOW(window), "GTK window");

    gtk_widget_show_all(window);
    gtk_main();
    receive_data_thread.join();
    update_image_thread.join();    

    // Fermer la soket
    close(sockfd);
    return 0;
}