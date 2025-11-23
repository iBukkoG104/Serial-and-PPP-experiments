// See https://aka.ms/new-console-template for more information
using System;
using System.IO;
using System.IO.Ports;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Net;

static class Program {
    static void Main(string[] args) {

		const int BAUDRATE = 115200;

        WebClient wc = new WebClient();
        SerialPort Serial = new SerialPort("/dev/ttyACM0", BAUDRATE);
        string text = "";

        if (!Serial.IsOpen) {
            Serial.Open();
        } Task.Delay(1000).Wait();

		while (text != "!!EXIT") {
			Console.WriteLine("Inserire comando: ([S]crivere)(Richiesta [H]TTP)([E]xit)");
			string r = Console.ReadLine();
            
            if (r == "") {}
            if (r == "E" || r == "e")return;
            else if(r == "S" || r == "s") {
                Console.WriteLine("Inserire testo: ");
                text = Console.ReadLine();      
                for (int i = 0; i < text.Length; i++) {
                    if(text[i] != '\\') {
                        Serial.Write(Convert.ToString(text[i]));
                    } else {
                        if(text[i+1] == 'n'){
                            Serial.Write(Convert.ToString('\n'));
                            i++;
                        } else if(text[i+1] == 'r'){
                            Serial.Write(Convert.ToString('\r'));
                            i++;
                        }
                    }
                }
    
            } else if (r == "H" || r == "h") {
                Console.WriteLine("Inserire url: ");
                string url = Console.ReadLine();
                string result = wc.DownloadString(url);

                Serial.Write('\n'.ToString()); Serial.Write(result);
            }
            
       } Serial.Close();     
    }
}