#include <iostream>
#include <cairo.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <gio/gfile.h>
#include <tinyxml2.h>
#include <chrono>
#include <cstring>
#include <thread>

using namespace tinyxml2;

static void do_drawing(cairo_t *);
static void do_drawing_svg(cairo_t *);

RsvgHandle *svg_handle;
RsvgRectangle viewport;

XMLElement* findElementSvg(XMLElement* element, const char* attribute) {
    XMLElement* elementParent;
    for (XMLElement* childAttribute = element->FirstChildElement(); childAttribute != NULL; childAttribute = childAttribute->NextSiblingElement()) {
        if (strcmp(childAttribute->Value(), "circle") == 0) {
            return childAttribute;
            const char* target_cx = childAttribute->Attribute("cx");
            const char* target_cy = childAttribute->Attribute("cy");
            if (strcmp(childAttribute->NextSiblingElement("driven")->Attribute("by"), attribute) == 0) {
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
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{      
  do_drawing_svg(cr);

  return FALSE;
}


void update_image(XMLDocument& svg_data, GtkWidget *darea) {
    const auto delay = std::chrono::milliseconds(50); // attente de 50ms entre chaque mise envoi
    XMLElement *svg_sun = svg_data.FirstChildElement("svg");
    XMLElement *circle_element = findElementSvg(svg_sun, "soleil");
    double cx = 50;
    double cy = 25;
    double delta = 0.1;
    for (size_t i = 0; i < 1000; i++) {
        std::this_thread::sleep_for(delay);
        cx += delta;
        cy += delta;
        circle_element->SetAttribute("cx",  cx);
        circle_element->SetAttribute("cy",  cy);
        // Recreate svg_handle and get new viewport
        XMLPrinter printer;
        svg_data.Print(&printer);
        RsvgHandle *new_svg_handle = rsvg_handle_new_from_data((const unsigned char*) printer.CStr(), printer.CStrSize(), NULL);
        RsvgRectangle new_viewport;
        rsvg_handle_get_geometry_for_element(new_svg_handle, NULL, &new_viewport, NULL, NULL);
        
        // Set new attributes for the circle element

        
        // Update the svg_handle and viewport
        if (svg_handle) {
            g_object_unref(svg_handle);
        }
        svg_handle = new_svg_handle;
        viewport = new_viewport;
        
        // Redraw the window
        gtk_widget_queue_draw(darea);
    }
}

static void do_drawing_svg(cairo_t * cr)
{
  rsvg_handle_render_document (svg_handle, cr, &viewport, NULL);
}


int main(int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *darea;

  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  darea = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(window), darea);

  XMLDocument svg_data;
  svg_data.LoadFile("maison.svg");
//   XMLElement *rectangle1 = svg_data.FirstChildElement("svg")->FirstChildElement("g")->FirstChildElement("rect");
//   rectangle1->SetAttribute("style", "fill:#000000");
  std::thread update_image_thread(update_image, std::ref(svg_data), darea);
  update_image_thread.detach();

  XMLPrinter printer;
  svg_data.Print(&printer);
  svg_handle = rsvg_handle_new_from_data ((const unsigned char*) printer.CStr(), printer.CStrSize(), NULL);
  rsvg_handle_get_geometry_for_element  (svg_handle, NULL, &viewport, NULL, NULL);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL); 
  g_signal_connect(window, "destroy",G_CALLBACK(gtk_main_quit), NULL);

  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), viewport.width, viewport.height); 
  gtk_window_set_title(GTK_WINDOW(window), "GTK window");
  gtk_widget_show_all(window);
  gtk_main();
  update_image_thread.join();

  return 0;
}