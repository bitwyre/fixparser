#include <string>
#include <string_view> 
#include <iostream> 
#include <vector>
#include <utility>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <pugixml.hpp>

#define SOH '|'

namespace fixparser {

using FixMessage = std::string;

    enum class FixStd : char {
        FIX44
    };

    std::vector<std::pair<pugi::xml_node,std::string>> header_;
    std::vector<std::pair<pugi::xml_node,std::string>> body_;
    std::vector<std::pair<pugi::xml_node,std::string>> trailer_;

    [[nodiscard]] auto split(const std::string s, const char delimiter) noexcept -> std::vector<std::string> {
        
        std::vector<std::string> internal;
        std::stringstream ss(s); // Turn the string into a stream.
        std::string tok;
        
        while(getline(ss, tok, delimiter)) {
            internal.emplace_back(tok);
        }
        
        return internal;
    }
 
    auto categorize(const pugi::xml_node& node, std::string_view nodeValue, const pugi::xml_document& fixSpec) noexcept -> void {

        auto headers = fixSpec.child("fix").child("header");
        auto trailers = fixSpec.child("fix").child("trailer");

        bool foundInTrailer = false;
        auto foundInHeader = headers.find_child_by_attribute("field","name",node.attribute("name").as_string());

        if(!foundInHeader){
            foundInTrailer = trailers.find_child_by_attribute("field","name",node.attribute("name").as_string());

            if( foundInTrailer ){
                trailer_.emplace_back( std::make_pair(node, nodeValue) );
            }
        }else{
            header_.emplace_back( std::make_pair(node, nodeValue) );
        }

        if(!foundInHeader && !foundInTrailer){
            body_.emplace_back( std::make_pair(node, nodeValue) );
        }

    }


    auto prettyPrint(std::string_view sv) -> void{
        
        std::cout << "RAW " << "\n";
        std::cout << sv << "\n\n\n";

        std::cout << "HEADER " << "\n\n";

        if( header_.empty() ){
            std::cout << "No Header" <<"\n";
        }

        for(const auto& elemHeader: header_){

            auto [elem,value] = elemHeader;

            std::cout << elem.attribute("number").as_string() << "\t\t" << elem.attribute("name").as_string() << ": " << value << "\n"; 
        }

        std::cout << "\n\nBODY " << "\n\n";

        if( body_.empty()){
            std::cout <<"No body"<< "\n\n";
        }

        for(const auto& elemBody: body_){

            auto [elem,value] =  elemBody;
            std::cout << elem.attribute("number").as_string() << "\t\t" << elem.attribute("name").as_string() << ": "<< value <<"\n"; 
        }

        std::cout << "\n\nTRAILER " << "\n\n"; 

        if( trailer_.empty()){
            std::cout <<"No TRAILER"<< "\n\n";
        }


        for(const auto& elemTrailer: trailer_){
            auto [elem,value] = elemTrailer;
            std::cout << elem.attribute("number").as_string() << "\t\t" << elem.attribute("name").as_string() << ": " << value << "\n"; 
        }

    }

    auto fixToHuman(const FixStd fixStd, const FixMessage msg) noexcept -> void {

        auto mappedVersion = [&fixStd = std::as_const(fixStd)](){
            switch (fixStd){
            case FixStd::FIX44 :
                return "FIX44";
                break;
            default:
                return "FIX44";
                break;
            }
        }();

        std::string source = "spec/";
                    source += mappedVersion;
                    source += ".xml";

        pugi::xml_document fixSpec;

        pugi::xml_parse_result result = fixSpec.load_file(source.c_str());

        if( result ){ 
            
            auto splittedMsg = split(msg, SOH);
            auto fields = fixSpec.child("fix").child("fields");
            
            for(const auto& s : splittedMsg){
                
                 auto attrs = split(s,'=');
                 auto foundElem = fields.find_child_by_attribute("field","number",attrs.front().c_str());

                 if( foundElem ){
                     categorize(foundElem, attrs.back(), fixSpec);
                 }else{
                     std::cout << "Element with Tag="<< attrs.front() << " Not found" << "\n";
                 }
            }

            prettyPrint(msg);

        }else{
            std::cout << "An error occured: ";
            std::cout << result.description() << "\n";
        }

    }


}// namespace fixparser