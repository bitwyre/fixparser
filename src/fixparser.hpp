#include <string>
#include <cstring>
#include <string_view> 
#include <iostream> 
#include <vector>
#include <utility>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <type_traits> 
#include <pugixml.hpp>

#define SOH '|'

namespace fixparser {

namespace fs = std::filesystem;
// Defining the necessary types to hold a FIX message 
struct Field{
    std::string fieldName_;
    std::string value_;
    std::uint16_t number_{};
    bool isRequired_{};
    bool isComponent_{};
    bool isInGroup_{};
};

struct Group{
    std::string groupName_;
    bool isRequired_{};
    std::vector<Field> grpFields_;
};

struct Component{
    Group group_;
};
struct Message{
    std::string msgName_;
    std::string msgType_;
    std::string msgCat_;
    std::vector<Field> msgFields_;
}; 

struct Value{
    std::string enumValue_;
    std::string description_;
};

struct Tag{
    std::uint16_t number_{};
    std::string name_;
    std::string type_;
    std::string value_;
    std::vector<Value> tagValues_;
};

struct Header{
    std::vector<Field> headerFields_;
    Group grp_;
};

struct Body{
    std::vector<Tag> tagValues_;
};

struct Trailer{
    std::vector<Field> trailer_;
};

struct FixMessage{
    Header header_;
    Body body_;
    Trailer trailer_;
};

enum class FixStd : char {
    FIX44
};

template <typename T,typename S=std::enable_if_t< std::is_convertible_v<T, std::string>, std::string> >
[[nodiscard]] constexpr auto split(T&& str, const char delimiter) noexcept -> std::vector<S> {

  auto strToConvert = std::string(std::forward<T>(str));
  std::vector<S> internal;

  std::stringstream ss( std::forward<S>(strToConvert)); // Turn the string into a stream.
  std::string tok;

  while (getline(ss, tok, delimiter)) {
    internal.emplace_back(tok);
  }

  return internal;
}

template<typename T>
constexpr auto printFieldImpl(T&& field, std::false_type) -> void {

    std::cout <<  field.number_ << "\t\t"; 
    std::cout << field.name_ << ": " << field.value_ << "\n\n";
}

template<typename T>
constexpr auto printFieldImpl(T&& field, std::true_type) -> void {

    std::cout << field.number_<< "\t\t";
    std::cout << field.fieldName_<< ": " << field.value_ << "\n\n";
}

template<typename T>
constexpr auto printField(T&& field) -> void {
    printFieldImpl( std::forward<T>(field), std::is_same< std::decay_t<T> , Field>() );
}
/**
 * Pretty print a FixMessage
 **/
template<typename T>
constexpr auto prettyPrint(T&& fixMsg) -> void {

    static_assert( std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, FixMessage>);

    // Print the header 
    std::cout << "HEADER"  << "\n";
    for(const auto& field: fixMsg.header_.headerFields_ ){
        printField( field );
    }
    // Print the body
    std::cout << "BODY"  << "\n";
    for(const auto& field: fixMsg.body_.tagValues_ ){
        printField( field );
    }
    // Print the trailer 
    std::cout << "Trailer"  << "\n";
    for(const auto& field: fixMsg.trailer_.trailer_ ){
        printField( field );
    }
}

template<typename T,typename=std::enable_if< !std::is_integral_v<T> > >
constexpr auto categorize(T&& vec, const FixStd fixStd) -> std::pair<FixMessage,bool> {

    FixMessage fixMsg;
    Header fixHeader;
    Body fixBody;
    Trailer fixTrailer;

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

    auto currentDir = fs::current_path();
    std::string source = currentDir;
                source += "/spec/";
                source += mappedVersion;
                source += ".xml";

    pugi::xml_document fixSpec;
    pugi::xml_parse_result result = fixSpec.load_file(source.c_str());

    if( result ){ 

        for(auto& tagValue : vec){

            auto tagValueVec = split(tagValue, '=');

            auto headers = fixSpec.child("fix").child("header");
            auto trailers = fixSpec.child("fix").child("trailer");
            auto fields = fixSpec.child("fix").child("fields");

            auto isField = fields.find_node([&tagValueVec](auto& node){
                return 
                std::strcmp(node.attribute("number").as_string(), tagValueVec.front().c_str() ) == 0;
            });

            if( isField ){
                 Field f;
                 f.number_ = isField.attribute("number").as_int();
                 f.value_ = tagValueVec.back();

                auto nodeFoundInHeader = headers.find_node([&isField](auto& node){
                    return 
                    std::strcmp(node.attribute("name").as_string(), isField.attribute("name").as_string() ) == 0;
                });

                if( nodeFoundInHeader ){
                    if( std::strcmp( nodeFoundInHeader.parent().name(), "group") ){
                        // The node is in a <group> node
                        f.isInGroup_ = true;
                    }
                    
                    f.fieldName_ = static_cast<std::string>(nodeFoundInHeader.attribute("name").as_string());
                    f.isComponent_ = false;
                    f.isRequired_ = nodeFoundInHeader.attribute("required").as_int() != 0x4E ? true : false;

                    fixHeader.headerFields_.emplace_back(f);
                }

                if( !nodeFoundInHeader ){

                    auto nodeFoundInTrailer = trailers.find_node([&isField](auto& node){
                        return 
                        std::strcmp(node.attribute("name").as_string(), isField.attribute("name").as_string() ) == 0;
                    });

                    if( nodeFoundInTrailer ){
                        f.fieldName_ = static_cast<std::string>(nodeFoundInTrailer.attribute("name").as_string());
                        f.isComponent_ = false;
                        f.isInGroup_ = false;
                        f.isRequired_ = nodeFoundInTrailer.attribute("required").as_int() != 0x4E ? true : false;

                        fixTrailer.trailer_.emplace_back( f );
                    }else{

                        // The node is in the body in this case so we construct a Tag object
                        // The question about the difference between Tag and Field may arise as this point
                        Tag tag ;

                        tag.name_ = static_cast<std::string>( isField.attribute("name").as_string() );
                        tag.number_ = isField.attribute("number").as_int();
                        tag.type_ = static_cast<std::string>( isField.attribute("type").as_string() );
                        tag.value_ = tagValueVec.back();
                        // If the field has some set of values we retrieve them

                        for( const auto& value: isField.children() ){
                            Value v;
                            v.description_ = static_cast<std::string>( value.attribute("type").as_string() );
                            v.enumValue_  = static_cast<std::string>( value.attribute("enum").as_string() );

                            tag.tagValues_.emplace_back( v );
                        }

                        fixBody.tagValues_.emplace_back( tag );
                    }

                }

            }else{
                
                // The field is not a correct field, means we didn't found an attribute number=x
                // This results in parsing error

                std::cout << "Field with tag="<< tagValueVec.front() << " Not found\n";
            }
 
        }

        fixMsg.header_ = fixHeader;
        fixMsg.body_ = fixBody;
        fixMsg.trailer_ = fixTrailer;

        return std::make_pair(fixMsg, false);
    }else{

        std::cerr << "Cannot open the FIX spec file" <<"\n";
        return { FixMessage{}, true};
    }
}

/**
 * @brief Check the message validity
 * @return true if the body length is correct false otherwise
*/
template <typename T,typename=std::enable_if_t<std::is_convertible_v<T,std::string> > >
constexpr auto checkMsgValidity(T&& message) noexcept -> bool {

    auto splittedMsg = split( std::forward<T>(message), SOH);

    auto [fixMessage, error] = categorize( splittedMsg, FixStd::FIX44);
 
    prettyPrint( fixMessage );
   
    return error;

}

}// namespace fixparser