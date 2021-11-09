#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Magnum.h>

#include <Magnum/Ui/Anchor.h>
#include <Magnum/Ui/Button.h>
#include <Magnum/Ui/Label.h>
#include <Magnum/Ui/Plane.h>
#include <Magnum/Ui/UserInterface.h>
#include <Magnum/Ui/ValidatedInput.h>
#include <Magnum/Text/Alignment.h>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Interconnect/Receiver.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;
using namespace Math::Literals;


/* Base UI plane */
constexpr Vector2 WidgetSize{80, 32};
const std::regex FloatValidator{R"(-?\d+(\.\d+)?)"};

struct BaseUiPlane: Ui::Plane {
    explicit BaseUiPlane(Ui::UserInterface& ui):
        Ui::Plane{ui, Ui::Snap::Top|Ui::Snap::Bottom|Ui::Snap::Left|Ui::Snap::Right, 0, 16, 128},
        metalness{*this, {Ui::Snap::Top|Ui::Snap::Right, WidgetSize}, FloatValidator, "0.5", 5},
        roughness{*this, {Ui::Snap::Bottom, metalness, WidgetSize}, FloatValidator, "0.25", 5},
        f0{*this, {Ui::Snap::Bottom, roughness, WidgetSize}, FloatValidator, "0.25", 5},

        apply{*this, {Ui::Snap::Bottom|Ui::Snap::Right, WidgetSize}, "Apply", Ui::Style::Primary},
        reset{*this, {Ui::Snap::Top, apply, WidgetSize}, "Reset", Ui::Style::Danger}
    {
        Ui::Label{*this, {Ui::Snap::Left, metalness}, "Metalness", Text::Alignment::MiddleRight};
        Ui::Label{*this, {Ui::Snap::Left, roughness}, "Roughness", Text::Alignment::MiddleRight};
        Ui::Label{*this, {Ui::Snap::Left, f0}, "ƒ₀", Text::Alignment::MiddleRight};

        Ui::Label{*this, {Ui::Snap::Bottom|Ui::Snap::Left, WidgetSize},
            "Use WASD + mouse to move, (Shift +) M/R/F to change parameters.",
            Text::Alignment::MiddleLeft};
    }

    Ui::ValidatedInput metalness,
        roughness,
        f0;
    Ui::Button apply,
        reset;
};

class UIExample: public Platform::Application, public Interconnect::Receiver
{
    private:
        float _metalness;
        float _roughness;
        float _f0;

        GL::Mesh mesh;
        Vector3 lightPosition;
        Matrix4 transformation, projection;
        Color3 activeShape;
        Shaders::Phong shader;

    public:
        explicit UIExample(const Arguments& arguments);

        void apply();
        void reset();
        void enableApplyButton(const std::string&);

    private:
        void drawEvent() override;
        void mousePressEvent(MouseEvent& event) override;
        void mouseReleaseEvent(MouseEvent& event) override;
        void keyPressEvent(KeyEvent& event) override;
        void textInputEvent(TextInputEvent& event) override;
        
    /* UI */
        Containers::Optional<Ui::UserInterface> _ui;
        Containers::Optional<BaseUiPlane> _baseUiPlane;

};

UIExample::UIExample(const Arguments& arguments): Platform::Application{arguments, Configuration{}
        .setTitle("Magnum UI Example")}
{
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    Trade::MeshData cube = Primitives::cubeSolid();

    GL::Buffer vertices;
    vertices.setData(MeshTools::interleave(cube.positions3DAsArray(),
                                           cube.normalsAsArray()));

    std::pair<Containers::Array<char>, MeshIndexType> compressed =
        MeshTools::compressIndices(cube.indicesAsArray());
    GL::Buffer indices;
    indices.setData(compressed.first);

    mesh.setPrimitive(cube.primitive())
        .setCount(cube.indexCount())
        .addVertexBuffer(std::move(vertices), 0, Shaders::Phong::Position{},
                                                 Shaders::Phong::Normal{})
        .setIndexBuffer(std::move(indices), 0, compressed.second);

    lightPosition = {1.4f, 1.0f, 2.0f};
    transformation = Matrix4::scaling({.1f, .1f, .1f}) * Matrix4::rotationX(30.0_degf)*Matrix4::rotationY(40.0_degf);
    projection =
        Matrix4::perspectiveProjection(
            35.0_degf, Vector2{windowSize()}.aspectRatio(), 0.01f, 150.0f)*
        Matrix4::translation({-5.0f, -22.0f, -130.0f});

    /* Create the UI */
    _ui.emplace(Vector2{windowSize()}/dpiScaling(), windowSize(), framebufferSize(), Ui::mcssDarkStyleConfiguration(), "ƒ₀");
    Interconnect::connect(*_ui, &Ui::UserInterface::inputWidgetFocused, *this, &UIExample::startTextInput);
    Interconnect::connect(*_ui, &Ui::UserInterface::inputWidgetBlurred, *this, &UIExample::stopTextInput);

    /* Base UI plane */
    _baseUiPlane.emplace(*_ui);
    Interconnect::connect(_baseUiPlane->metalness, &Ui::Input::valueChanged, *this, &UIExample::enableApplyButton);
    Interconnect::connect(_baseUiPlane->roughness, &Ui::Input::valueChanged, *this, &UIExample::enableApplyButton);
    Interconnect::connect(_baseUiPlane->f0, &Ui::Input::valueChanged, *this, &UIExample::enableApplyButton);
    Interconnect::connect(_baseUiPlane->apply, &Ui::Button::tapped, *this, &UIExample::apply);
    Interconnect::connect(_baseUiPlane->reset, &Ui::Button::tapped, *this, &UIExample::reset);

    apply();    
}

void UIExample::drawEvent()
{
    GL::defaultFramebuffer.clear(
        GL::FramebufferClear::Color|GL::FramebufferClear::Depth);
    
    shader.setLightPositions({lightPosition})
        .setDiffuseColor(activeShape)
        .setAmbientColor(Color3::fromHsv({activeShape.hue(), 1.0f, 0.3f}))
        .setTransformationMatrix(Matrix4::translation({0.0f, 0.0f, 0.0f}))
        .setNormalMatrix(transformation.normalMatrix())
        .setProjectionMatrix(projection)
        .draw(mesh);

    /* Draw the UI */
    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::One, GL::Renderer::BlendFunction::OneMinusSourceAlpha);
    _ui->draw();
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::One, GL::Renderer::BlendFunction::One);
    GL::Renderer::disable(GL::Renderer::Feature::Blending);

    swapBuffers();
    redraw();
}

void UIExample::enableApplyButton(const std::string&)
{
    _baseUiPlane->apply.setEnabled(Ui::ValidatedInput::allValid({
        _baseUiPlane->metalness,
        _baseUiPlane->roughness,
        _baseUiPlane->f0}));
}

void UIExample::apply()
{
    _metalness = Math::clamp(std::stof(_baseUiPlane->metalness.value()), 0.1f, 1.0f);
    _roughness = Math::clamp(std::stof(_baseUiPlane->roughness.value()), 0.1f, 1.0f);
    _f0 = Math::clamp(std::stof(_baseUiPlane->f0.value()), 0.1f, 1.0f);

    shader.setShininess(1 - _roughness);

    /* Set the clamped values back */
    _baseUiPlane->metalness.setValue(Utility::formatString("{:.5}", _metalness));
    _baseUiPlane->roughness.setValue(Utility::formatString("{:.5}", _roughness));
    _baseUiPlane->f0.setValue(Utility::formatString("{:.5}", _f0));
}

void UIExample::reset() {
    _baseUiPlane->metalness.setValue("0.5");
    _baseUiPlane->roughness.setValue("0.25");
    _baseUiPlane->f0.setValue("0.25");

    apply();
}

void UIExample::textInputEvent(TextInputEvent& event)
{
    if(isTextInputActive() && _ui->focusedInputWidget() && _ui->focusedInputWidget()->handleTextInput(event))
        redraw();
}



void UIExample::keyPressEvent(KeyEvent& event)
{
    /* If an input is focused, pass the events only to the UI */
    if(isTextInputActive() && _ui->focusedInputWidget()) 
    {
        if(!_ui->focusedInputWidget()->handleKeyPress(event)) return;
    }

    redraw();
}

void UIExample::mousePressEvent(MouseEvent& event) {
    if(!_ui->handlePressEvent(event.position())) return;

    redraw();
}

void UIExample::mouseReleaseEvent(MouseEvent& event) {
    if(_ui->handleReleaseEvent(event.position()))
        redraw();
}

MAGNUM_APPLICATION_MAIN(UIExample)