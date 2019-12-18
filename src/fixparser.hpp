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
    std::vector<Value> tagValues_; // Set of values that a specific Tag can take, this maybe empty for some tags
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
    std::string rawMsg_;
};

enum class FixStd : char {
    FIX44
};

struct Error{
    std::string errMsg_;
};
struct ErrorBag{
    std::vector<Error> errors_;

    auto isEmpty() -> bool {
        return errors_.empty();
    }
};

auto operator<<(std::ostream& os, ErrorBag errBag) -> std::ostream&{
    if( errBag.isEmpty() ){
        os << "No errors found" << "\n";
        return os;
    }

     os << "A total of " << errBag.errors_.size() << " error(s) found \n\n";
    for(const auto& err: errBag.errors_ ){
        os << err.errMsg_ << "\n";
    }

    return os;
}

ErrorBag errorBag{};
pugi::xml_document fixSpec;
FixMessage fixMessage;

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

    if( !errorBag.errors_.empty() ){
        std::cerr << "The message contains some errors, please check the FIX specification to get the list of correct fields \n\n";
        std::cerr << errorBag << "\n\n";
        return;
    }
    // Print the header 
    std::cout << "HEADER"  << "\n\n";
    for(const auto& field: fixMsg.header_.headerFields_ ){
        printField( field );
    }
    // Print the body
    std::cout << "BODY"  << "\n\n";
    for(const auto& field: fixMsg.body_.tagValues_ ){
        printField( field );
    }
    // Print the trailer 
    std::cout << "Trailer"  << "\n\n";
    for(const auto& field: fixMsg.trailer_.trailer_ ){
        printField( field );
    }
}

template<typename T,typename=std::enable_if< !std::is_integral_v<T> > >
[[nodiscard]] constexpr auto categorize(T&& vec, const FixStd fixStd) -> std::pair<FixMessage,ErrorBag> {

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
                std::string err = "Field with tag=";
                            err+= tagValueVec.front();
                            err+= " not found";

                errorBag.errors_.emplace_back( Error{std::move(err)} );
            }
 
        }

        fixMsg.header_ = fixHeader;
        fixMsg.body_ = fixBody;
        fixMsg.trailer_ = fixTrailer;

        return std::make_pair(fixMsg, errorBag);
    }else{

        errorBag.errors_.emplace_back( Error{"Cannot open the FIX spec file."} );
        return std::make_pair( FixMessage{}, errorBag );
    }
}

/**
 * @brief Check the message validity
 * @return true if the messge is correct false otherwise
*/
template <typename T,typename=std::enable_if_t<std::is_convertible_v<T, std::string> > >
constexpr auto checkMsgValidity(T&& message) noexcept -> bool {

    auto splittedMsg = split( message, SOH);

    auto [fixMg, error] = categorize( splittedMsg, FixStd::FIX44);

    if( !error.isEmpty() ){
        return false;
    }

    auto requiredFieldsPresent = checkRequiredFields( fixMg );

    if( requiredFieldsPresent ){
        return false;
    }

    auto bodyLengthCorrect = checkBodyLength( fixMg );

    if( !bodyLengthCorrect ){
        return false;
    }

    auto checkSumCorrect = checkCheckSum( fixMg );
    
    if( !checkSumCorrect ){
        return false;
    }

    fixMessage = std::move(fixMg);
    fixMessage.rawMsg_ = std::forward<T>(message); 
    
    return true;

}

/**
 * @brief Check for required fields in the message 
 * @return true if the message contents errors (means doesn't have the required fields), false otherwise
*/
template <typename T,typename=std::enable_if_t<std::is_same_v<std::decay_t<T>, FixMessage> > >
constexpr auto checkRequiredFields(T&& message) noexcept -> bool{

    auto headerFields = fixSpec.child("fix").child("header").children();
    auto trailerFields = fixSpec.child("fix").child("trailer").children();

    bool hasErrors{};
    std::string msgType{};

    // Check for required fields in the header
    for(const auto& child: headerFields ){

        if( std::strcmp( child.attribute("required").as_string(), "Y") == 0 ){

            // Check if the field is present in the header
            auto isFieldPresent = std::find_if( message.header_.headerFields_.begin(), 
                                                message.header_.headerFields_.end(),
                                                [&child](auto& field){
                                                    return field.fieldName_ == child.attribute("name").as_string();
                                                });

            if( isFieldPresent == message.header_.headerFields_.end() ){
                std::string errMsg = "HEADER: the tag with name=";
                            errMsg += child.attribute("name").as_string();
                            errMsg += " is required";

                errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                hasErrors = true;
            }else{

            // We retrieve and store the message type at this level 
                auto f = *isFieldPresent;

                if( f.number_ == 35 ){
                    msgType = f.value_;
                }
            }

        }
    }
    
    // Check the required fields in the body
    // NOTE: There are some conditional required fields, not dealing with them as of now
    // NOTE: Some required fields depends on the message type tag 35=MsgType

    auto isCorrectMsgType = fixSpec.child("fix")
                                   .child("messages")
                                   .find_child_by_attribute("message","msgtype", msgType.c_str() );

    if( !isCorrectMsgType ){
        std::string errMsg("The message type is invalid");

        errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
        hasErrors = true;
    }else{
        
        // We can now check for required fields for the specified message
        // @TODO: Deal with the case of required components

        auto msgField = std::move(isCorrectMsgType);

        for(const auto& child: msgField.children() ){

            if( std::strcmp( child.attribute("required").as_string(), "Y") == 0 ){

                // Check if the field is present in the header
                auto isFieldPresent = std::find_if( message.body_.tagValues_.begin(), 
                                                    message.body_.tagValues_.end(),
                                                    [&child](auto& tag){
                                                        return tag.name_ == child.attribute("name").as_string();
                                                    });

                if( isFieldPresent == message.body_.tagValues_.end() ){
                    std::string errMsg = "BODY: the tag with name=";
                                errMsg += child.attribute("name").as_string();
                                errMsg += " is required";

                    errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                    hasErrors = true;
                }

            }
        }

    }

    // Check the required fields for the trailer 

    for(const auto& child: trailerFields ){

        if( std::strcmp( child.attribute("required").as_string(), "Y") == 0 ){

            // Check if the field is present in the header
            auto isFieldPresent = std::find_if( message.trailer_.trailer_.begin(), 
                                                message.trailer_.trailer_.end(),
                                                [&child](auto& field){
                                                    return field.fieldName_ == child.attribute("name").as_string();
                                                });

            if( isFieldPresent == message.trailer_.trailer_.end() ){
                std::string errMsg = "TRAILER: the tag with name=";
                            errMsg += child.attribute("name").as_string();
                            errMsg += " is required";

                errorBag.errors_.emplace_back( Error{std::move(errMsg)} );
                hasErrors = true;
            }

        }
    }

    return hasErrors;
}


/**
 * @brief Check the message body length
 * @return true if the body length is correct false otherwise
*/
template <typename T,typename=std::enable_if_t<std::is_same_v<std::decay_t<T>, FixMessage> > >
constexpr auto checkBodyLength(T&& message) noexcept -> bool {

    int computedLength{};

    for(const auto& msg : message.body_.tagValues_ ){
        computedLength += static_cast<int>( msg.value_.size() ) + 
                          static_cast<int>( (std::to_string( msg.number_ )).size() ) + 2; // 2 = the SOH plus the '=' in the message
    }

    for(const auto& msg : message.header_.headerFields_ ){

        // We are excluding the tag 8 which contains the FIX version and the tag 9 which contains the body length
        if( msg.number_ != 9 && msg.number_ != 8 ){
            computedLength += static_cast<int>( msg.value_.size() ) + 
                            static_cast<int>( (std::to_string( msg.number_ )).size() ) + 2; // 2 = the SOH plus the '=' in the message
        }
   
    }

    auto bodyLengthElem = std::find_if( message.header_.headerFields_.begin(),
                                       message.header_.headerFields_.end(),
                                        [](auto& elem){
                                            return elem.number_ == 9;
                                        });

    if( bodyLengthElem != message.header_.headerFields_.end() ){
        auto f = *bodyLengthElem;
        auto areOfEqualLength = std::atoi( f.value_.c_str() ) == computedLength;

        if( !areOfEqualLength ){
            std::string errorMsg = "Message body length mismatch.\nExpected: ";
                        errorMsg += std::to_string(computedLength);
                        errorMsg += "\nGot: ";
                        errorMsg += f.value_;
            errorBag.errors_.emplace_back( Error{ std::move(errorMsg)} );
        }
        return areOfEqualLength;
    }

    return false;
}

/**
 * @brief Check if the message checksum is correct
 * @return true if the checksum is correct false otherwise
 **/

template<typename T,typename=std::enable_if_t<std::is_same_v<std::decay_t<T>, FixMessage> > >
constexpr auto checkCheckSum(T&& message) noexcept -> bool {

    // We assume that the trailer only contains one field which is the checksum
    // While checking the FIX spec we discovered that other fields in the trailer are deprecated

    auto csSize = message.trailer_.trailer_.at(0).value_.size() == 3;

    if( !csSize ){
        errorBag.errors_.emplace_back( Error{"The checksum size is invalide. It should be 3"});
        return false;
    }

    uint16_t computedCheckSum{};
    int countSoh{0};

    // The loop is going till the size() - 7, 7=number of characters in the trailing tag of the message
    for(int i{0}; i < message.rawMsg_.size() - 7; ++i){
        
        if( SOH != message.rawMsg_.at(i) ){
            computedCheckSum += message.rawMsg_.at(i);
        }else{
            ++countSoh;
        }
    }

    auto computedCheckSumStr = "0"+ std::to_string((computedCheckSum+countSoh)%256);
    
    if( computedCheckSumStr != message.trailer_.trailer_.at(0).value_ ){

        std::string errMsg = "The message checksum is invalid.\nExpected: " + computedCheckSumStr + "\nGot: " + message.trailer_.trailer_.at(0).value_+ "\n";
        errorBag.errors_.emplace_back( Error{std::move(errMsg)});
        return false;
    }

    return true;
}


}// namespace fixparser