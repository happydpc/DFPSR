
#include "spriteAPI.h"
#include "Octree.h"
#include "../../../DFPSR/render/ITriangle2D.h"

namespace dsr {

struct SpriteConfig {
	int centerX, centerY; // The sprite's origin in pixels relative to the upper left corner
	int frameRows; // The atlas has one row for each frame
	int propertyColumns; // The atlas has one column for each type of information
	// The 3D model's bound in tile units
	//   The height image goes from 0 at minimum Y to 255 at maximum Y
	FVector3D minBound, maxBound;
	// Shadow shapes
	List<FVector3D> points; // 3D points for the triangles to refer to by index
	List<int32_t> triangleIndices; // Triangle indices stored in multiples of three integers
	// Construction
	SpriteConfig(int centerX, int centerY, int frameRows, int propertyColumns, FVector3D minBound, FVector3D maxBound)
	: centerX(centerX), centerY(centerY), frameRows(frameRows), propertyColumns(propertyColumns), minBound(minBound), maxBound(maxBound) {}
	explicit SpriteConfig(const ReadableString& content) {
		config_parse_ini(content, [this](const ReadableString& block, const ReadableString& key, const ReadableString& value) {
			if (string_length(block) == 0) {
				if (string_caseInsensitiveMatch(key, U"CenterX")) {
					this->centerX = string_toInteger(value);
				} else if (string_caseInsensitiveMatch(key, U"CenterY")) {
					this->centerY = string_toInteger(value);
				} else if (string_caseInsensitiveMatch(key, U"FrameRows")) {
					this->frameRows = string_toInteger(value);
				} else if (string_caseInsensitiveMatch(key, U"PropertyColumns")) {
					this->propertyColumns = string_toInteger(value);
				} else if (string_caseInsensitiveMatch(key, U"MinBound")) {
					this->minBound = parseFVector3D(value);
				} else if (string_caseInsensitiveMatch(key, U"MaxBound")) {
					this->maxBound = parseFVector3D(value);
				} else if (string_caseInsensitiveMatch(key, U"Points")) {
					List<ReadableString> values = string_split(value, U',');
					if (values.length() % 3 != 0) {
						throwError("Points contained ", values.length(), " values, which is not evenly divisible by three!");
					} else {
						this->points.clear();
						this->points.reserve(values.length() / 3);
						for (int v = 0; v < values.length(); v += 3) {
							this->points.push(FVector3D(string_toDouble(values[v]), string_toDouble(values[v+1]), string_toDouble(values[v+2])));
						}
					}
				} else if (string_caseInsensitiveMatch(key, U"TriangleIndices")) {
					List<ReadableString> values = string_split(value, U',');
					if (values.length() % 3 != 0) {
						throwError("TriangleIndices contained ", values.length(), " values, which is not evenly divisible by three!");
					} else {
						this->triangleIndices.clear();
						this->triangleIndices.reserve(values.length());
						for (int v = 0; v < values.length(); v++) {
							this->triangleIndices.push(string_toInteger(values[v]));
						}
					}
				} else {
					printText("Unrecognized key \"", key, "\" in sprite configuration file.\n");
				}
			} else {
				printText("Unrecognized block \"", block, "\" in sprite configuration file.\n");
			}
		});
	}
	// Add model as a persistent shadow caster in the sprite configuration
	void appendShadow(const Model& model) {
		points.reserve(this->points.length() + model_getNumberOfPoints(model));
		for (int p = 0; p < model_getNumberOfPoints(model); p++) {
			this->points.push(model_getPoint(model, p));
		}
		for (int part = 0; part < model_getNumberOfParts(model); part++) {
			for (int poly = 0; poly < model_getNumberOfPolygons(model, part); poly++) {
				int vertexCount = model_getPolygonVertexCount(model, part, poly);
				int vertA = 0;
				int indexA = model_getVertexPointIndex(model, part, poly, vertA);
				for (int vertB = 1; vertB < vertexCount - 1; vertB++) {
					int vertC = vertB + 1;
					int indexB = model_getVertexPointIndex(model, part, poly, vertB);
					int indexC = model_getVertexPointIndex(model, part, poly, vertC);
					triangleIndices.push(indexA); triangleIndices.push(indexB); triangleIndices.push(indexC);
				}
			}
		}
	}
	String toIni() {
		// General information
		String result = string_combine(
			U"; Sprite configuration file\n",
			U"CenterX=", this->centerX, "\n",
			U"CenterY=", this->centerY, "\n",
			U"FrameRows=", this->frameRows, "\n",
			U"PropertyColumns=", this->propertyColumns, "\n",
			U"MinBound=", this->minBound, "\n",
			U"MaxBound=", this->maxBound, "\n"
		);
		// Low-resolution 3D shape
		if (this->points.length() > 0) {
			string_append(result, U"Points=");
			for (int p = 0; p < this->points.length(); p++) {
				if (p > 0) {
					string_append(result, U", ");
				}
				string_append(result, this->points[p]);
			}
			string_append(result, U"\n");
			string_append(result, U"TriangleIndices=");
			for (int i = 0; i < this->triangleIndices.length(); i+=3) {
				if (i > 0) {
					string_append(result, U", ");
				}
				string_append(result, this->triangleIndices[i], U",", this->triangleIndices[i+1], U",", this->triangleIndices[i+2]);
			}
			string_append(result, U"\n");
		}
		return result;
	}
};

static ImageF32 scaleHeightImage(const ImageRgbaU8& heightImage, float minHeight, float maxHeight, const ImageRgbaU8& colorImage) {
	float scale = (maxHeight - minHeight) / 255.0f;
	float offset = minHeight;
	int width = image_getWidth(heightImage);
	int height = image_getHeight(heightImage);
	ImageF32 result = image_create_F32(width, height);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			float value = image_readPixel_clamp(heightImage, x, y).red;
			if (image_readPixel_clamp(colorImage, x, y).alpha > 127) {
				image_writePixel(result, x, y, (value * scale) + offset);
			} else {
				image_writePixel(result, x, y, -std::numeric_limits<float>::infinity());
			}
		}
	}
	return result;
}

struct SpriteFrame {
	IVector2D centerPoint;
	ImageRgbaU8 colorImage; // (Red, Green, Blue, _)
	ImageRgbaU8 normalImage; // (NormalX, NormalY, NormalZ, _)
	ImageF32 heightImage;
	SpriteFrame(const IVector2D& centerPoint, const ImageRgbaU8& colorImage, const ImageRgbaU8& normalImage, const ImageF32& heightImage)
	: centerPoint(centerPoint), colorImage(colorImage), normalImage(normalImage), heightImage(heightImage) {}
};

struct SpriteType {
public:
	IVector3D minBoundMini, maxBoundMini;
	List<SpriteFrame> frames;
	// TODO: Compress the data using a shadow-only model type of only positions and triangle indices in a single part.
	//       The shadow model will have its own rendering method excluding the color target.
	//       Shadow rendering can be a lot simpler by not calculating any vertex weights
	//         just interpolate the depth using addition, compare to the old value and write the new depth value.
	Model shadowModel;
public:
	// folderPath should end with a path separator
	SpriteType(const String& folderPath, const String& spriteName) {
		// Load the image atlas
		ImageRgbaU8 loadedAtlas = image_load_RgbaU8(string_combine(folderPath, spriteName, U".png"));
		// Load the settings
		const SpriteConfig configuration = SpriteConfig(string_load(string_combine(folderPath, spriteName, U".ini")));
		this->minBoundMini = IVector3D(
		  floor(configuration.minBound.x * ortho_miniUnitsPerTile),
		  floor(configuration.minBound.y * ortho_miniUnitsPerTile),
		  floor(configuration.minBound.z * ortho_miniUnitsPerTile)
		);
		this->maxBoundMini = IVector3D(
		  ceil(configuration.maxBound.x * ortho_miniUnitsPerTile),
		  ceil(configuration.maxBound.y * ortho_miniUnitsPerTile),
		  ceil(configuration.maxBound.z * ortho_miniUnitsPerTile)
		);
		int width = image_getWidth(loadedAtlas) / configuration.propertyColumns;
		int height = image_getHeight(loadedAtlas) / configuration.frameRows;
		for (int a = 0; a < configuration.frameRows; a++) {
			ImageRgbaU8 colorImage = image_getSubImage(loadedAtlas, IRect(0, a * height, width, height));
			ImageRgbaU8 heightImage = image_getSubImage(loadedAtlas, IRect(width, a * height, width, height));
			ImageRgbaU8 normalImage = image_getSubImage(loadedAtlas, IRect(width * 2, a * height, width, height));
			ImageF32 scaledHeightImage = scaleHeightImage(heightImage, configuration.minBound.y, configuration.maxBound.y, colorImage);
			this->frames.pushConstruct(IVector2D(configuration.centerX, configuration.centerY), colorImage, normalImage, scaledHeightImage);
		}
		// Create a model for rendering shadows
		if (configuration.points.length() > 0) {
			this->shadowModel = model_create();
			for (int p = 0; p < configuration.points.length(); p++) {
				model_addPoint(this->shadowModel, configuration.points[p]);
			}
			model_addEmptyPart(this->shadowModel, U"Shadow");
			for (int t = 0; t < configuration.triangleIndices.length(); t+=3) {
				model_addTriangle(this->shadowModel, 0, configuration.triangleIndices[t], configuration.triangleIndices[t+1], configuration.triangleIndices[t+2]);
			}
		}
	}
public:
	// TODO: Force frame count to a power of two or replace modulo with look-up tables in sprite configurations.
	int getFrameIndex(Direction direction) {
		const int frameFromDir[dir360] = {4, 1, 5, 2, 6, 3, 7, 0};
		return frameFromDir[correctDirection(direction)] % this->frames.length();
	}
};

// Global list of all sprite types ever loaded
List<SpriteType> types;

static int getSpriteFrameIndex(const Sprite& sprite, OrthoView view) {
	return types[sprite.typeIndex].getFrameIndex(view.worldDirection + sprite.direction);
}

void drawSprite(const Sprite& sprite, const OrthoView& ortho, const IVector2D& worldCenter, ImageF32 targetHeight, ImageRgbaU8 targetColor, ImageRgbaU8 targetNormal) {
	int frameIndex = getSpriteFrameIndex(sprite, ortho);
	const SpriteFrame* frame = &types[sprite.typeIndex].frames[frameIndex];
	IVector2D screenSpace = ortho.miniTilePositionToScreenPixel(sprite.location, worldCenter) - frame->centerPoint;
	float heightOffset = sprite.location.y * ortho_tilesPerMiniUnit;
	if (image_exists(targetColor)) {
		if (image_exists(targetNormal)) {
			draw_higher(targetHeight, frame->heightImage, targetColor, frame->colorImage, targetNormal, frame->normalImage, screenSpace.x, screenSpace.y, heightOffset);
		} else {
			draw_higher(targetHeight, frame->heightImage, targetColor, frame->colorImage, screenSpace.x, screenSpace.y, heightOffset);
		}
	} else {
		if (image_exists(targetNormal)) {
			draw_higher(targetHeight, frame->heightImage, targetNormal, frame->normalImage, screenSpace.x, screenSpace.y, heightOffset);
		} else {
			draw_higher(targetHeight, frame->heightImage, screenSpace.x, screenSpace.y, heightOffset);
		}
	}
}

// The camera transform for each direction
FMatrix3x3 ShadowCubeMapSides[6] = {
	FMatrix3x3::makeAxisSystem(FVector3D( 1.0f, 0.0f, 0.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D(-1.0f, 0.0f, 0.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 0.0f, 1.0f, 0.0f), FVector3D(0.0f, 0.0f, 1.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 0.0f,-1.0f, 0.0f), FVector3D(0.0f, 0.0f, 1.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 0.0f, 0.0f, 1.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 0.0f, 0.0f,-1.0f), FVector3D(0.0f, 1.0f, 0.0f))
};

// TODO: Move to the ortho API using a safe getter in modulo
FMatrix3x3 spriteDirections[8] = {
	FMatrix3x3::makeAxisSystem(FVector3D( 0.0f, 0.0f, 1.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 1.0f, 0.0f, 1.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 1.0f, 0.0f, 0.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 1.0f, 0.0f,-1.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D( 0.0f, 0.0f,-1.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D(-1.0f, 0.0f,-1.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D(-1.0f, 0.0f, 0.0f), FVector3D(0.0f, 1.0f, 0.0f)),
	FMatrix3x3::makeAxisSystem(FVector3D(-1.0f, 0.0f, 1.0f), FVector3D(0.0f, 1.0f, 0.0f))
};

struct CubeMapF32 {
	int resolution;           // The width and height of each shadow depth image or 0 if no shadows are casted
	AlignedImageF32 cubeMap;  // A vertical sequence of reciprocal depth images for the six sides of the cube
	ImageF32 cubeMapViews[6]; // Sub-images sharing their allocations with cubeMap as sub-images
	explicit CubeMapF32(int resolution) : resolution(resolution) {
		this->cubeMap = image_create_F32(resolution, resolution * 6);
		for (int s = 0; s < 6; s++) {
			this->cubeMapViews[s] = image_getSubImage(this->cubeMap, IRect(0, s * resolution, resolution, resolution));
		}
	}
	void clear() {
		image_fill(this->cubeMap, 0.0f);
	}
};

class PointLight {
public:
	FVector3D position; // The world-space center in tile units
	float radius;       // The light radius in tile units
	float intensity;    // The color's brightness multiplier (using float to allow smooth fading)
	ColorRgbI32 color;  // The color of the light (using integers to detect when the color is uniform)
	bool shadowCasting; // Casting shadows when enabled
public:
	PointLight(FVector3D position, float radius, float intensity, ColorRgbI32 color, bool shadowCasting)
	: position(position), radius(radius), intensity(intensity), color(color), shadowCasting(shadowCasting) {}
public:
	void renderSpriteShadow(CubeMapF32& shadowTarget, const Sprite& sprite, const FMatrix3x3& normalToWorld) const {
		if (sprite.shadowCasting) {
			Model model = types[sprite.typeIndex].shadowModel;
			if (model_exists(model)) {
				// Place the model relative to the light source's position, to make rendering in light-space easier
				Transform3D modelToWorldTransform = Transform3D(ortho_miniToFloatingTile(sprite.location) - this->position, spriteDirections[sprite.direction]);
				for (int s = 0; s < 6; s++) {
					Camera camera = Camera::createPerspective(Transform3D(FVector3D(), ShadowCubeMapSides[s] * normalToWorld), shadowTarget.resolution, shadowTarget.resolution);
					model_renderDepth(model, modelToWorldTransform, shadowTarget.cubeMapViews[s], camera);
				}
			}
		}
	}
	void renderSpriteShadows(CubeMapF32& shadowTarget, Octree<Sprite>& sprites, const FMatrix3x3& normalToWorld) const {
		IVector3D center = ortho_floatingTileToMini(this->position);
		IVector3D minBound = center - ortho_floatingTileToMini(radius);
		IVector3D maxBound = center + ortho_floatingTileToMini(radius);
		sprites.map(minBound, maxBound, [this, shadowTarget, normalToWorld](Sprite& sprite, const IVector3D origin, const IVector3D minBound, const IVector3D maxBound) mutable {
			this->renderSpriteShadow(shadowTarget, sprite, normalToWorld);
		});
	}
public:
	void illuminate(const OrthoView& camera, const IVector2D& worldCenter, OrderedImageRgbaU8& lightBuffer, const OrderedImageRgbaU8& normalBuffer, const AlignedImageF32& heightBuffer, const CubeMapF32& shadowSource) const {
		if (this->shadowCasting) {
			addPointLight(camera, worldCenter, lightBuffer, normalBuffer, heightBuffer, this->position, this->radius, this->intensity, this->color, shadowSource.cubeMap);
		} else {
			addPointLight(camera, worldCenter, lightBuffer, normalBuffer, heightBuffer, this->position, this->radius, this->intensity, this->color);
		}
	}
};

class DirectedLight {
public:
	FVector3D direction;    // The world-space direction
	float intensity;        // The color's brightness multiplier (using float to allow smooth fading)
	ColorRgbI32 color;      // The color of the light (using integers to detect when the color is uniform)
public:
	DirectedLight(FVector3D direction, float intensity, ColorRgbI32 color)
	: direction(direction), intensity(intensity), color(color) {}
public:
	void illuminate(const OrthoView& camera, const IVector2D& worldCenter, OrderedImageRgbaU8& lightBuffer, const OrderedImageRgbaU8& normalBuffer, bool overwrite = false) const {
		if (overwrite) {
			setDirectedLight(camera, lightBuffer, normalBuffer, this->direction, this->intensity, this->color);
		} else {
			addDirectedLight(camera, lightBuffer, normalBuffer, this->direction, this->intensity, this->color);
		}
	}
};

enum class BlockState {
	Unused,
	Ready,
	Dirty
};
class BackgroundBlock {
public:
	static const int blockSize = 512;
	static const int maxDistance = blockSize * 2;
	IRect worldRegion;
	int cameraId = 0;
	BlockState state = BlockState::Unused;
	OrderedImageRgbaU8 diffuseBuffer;
	OrderedImageRgbaU8 normalBuffer;
	AlignedImageF32 heightBuffer;
private:
	IVector3D getBoxCorner(const IVector3D& minBound, const IVector3D& maxBound, int cornerIndex) {
		assert(cornerIndex >= 0 && cornerIndex < 8);
		return IVector3D(
		  ((uint32_t)cornerIndex & 1u) ? maxBound.x : minBound.x,
		  ((uint32_t)cornerIndex & 2u) ? maxBound.y : minBound.y,
		  ((uint32_t)cornerIndex & 4u) ? maxBound.z : minBound.z
		);
	}
	// Pre-condition: diffuseBuffer must be cleared unless sprites cover the whole block
	void draw(Octree<Sprite>& sprites, const OrthoView& ortho) {
		image_fill(this->normalBuffer, ColorRgbaI32(128));
		image_fill(this->heightBuffer, -std::numeric_limits<float>::max());
		sprites.map(
		[ortho,this](const IVector3D& minBound, const IVector3D& maxBound){
			IVector2D corners[8];
			for (int c = 0; c < 8; c++) {
				corners[c] = ortho.miniTileOffsetToScreenPixel(getBoxCorner(minBound, maxBound, c));
			}
			if (corners[0].x < this->worldRegion.left()
			 && corners[1].x < this->worldRegion.left()
			 && corners[2].x < this->worldRegion.left()
			 && corners[3].x < this->worldRegion.left()
			 && corners[4].x < this->worldRegion.left()
			 && corners[5].x < this->worldRegion.left()
			 && corners[6].x < this->worldRegion.left()
			 && corners[7].x < this->worldRegion.left()) {
				return false;
			}
			if (corners[0].x > this->worldRegion.right()
			 && corners[1].x > this->worldRegion.right()
			 && corners[2].x > this->worldRegion.right()
			 && corners[3].x > this->worldRegion.right()
			 && corners[4].x > this->worldRegion.right()
			 && corners[5].x > this->worldRegion.right()
			 && corners[6].x > this->worldRegion.right()
			 && corners[7].x > this->worldRegion.right()) {
				return false;
			}
			if (corners[0].y < this->worldRegion.top()
			 && corners[1].y < this->worldRegion.top()
			 && corners[2].y < this->worldRegion.top()
			 && corners[3].y < this->worldRegion.top()
			 && corners[4].y < this->worldRegion.top()
			 && corners[5].y < this->worldRegion.top()
			 && corners[6].y < this->worldRegion.top()
			 && corners[7].y < this->worldRegion.top()) {
				return false;
			}
			if (corners[0].y > this->worldRegion.bottom()
			 && corners[1].y > this->worldRegion.bottom()
			 && corners[2].y > this->worldRegion.bottom()
			 && corners[3].y > this->worldRegion.bottom()
			 && corners[4].y > this->worldRegion.bottom()
			 && corners[5].y > this->worldRegion.bottom()
			 && corners[6].y > this->worldRegion.bottom()
			 && corners[7].y > this->worldRegion.bottom()) {
				return false;
			}
			return true;
		},
		[this, ortho](Sprite& sprite, const IVector3D origin, const IVector3D minBound, const IVector3D maxBound){
			drawSprite(sprite, ortho, -this->worldRegion.upperLeft(), this->heightBuffer, this->diffuseBuffer, this->normalBuffer);
		});
	}
public:
	BackgroundBlock(Octree<Sprite>& sprites, const IRect& worldRegion, const OrthoView& ortho)
	: worldRegion(worldRegion), cameraId(ortho.id), state(BlockState::Ready),
	  diffuseBuffer(image_create_RgbaU8(blockSize, blockSize)),
	  normalBuffer(image_create_RgbaU8(blockSize, blockSize)),
	  heightBuffer(image_create_F32(blockSize, blockSize)) {
		this->draw(sprites, ortho);
	}
	void update(Octree<Sprite>& sprites, const IRect& worldRegion, const OrthoView& ortho) {
		this->worldRegion = worldRegion;
		this->cameraId = ortho.id;
		image_fill(this->diffuseBuffer, ColorRgbaI32(0));
		this->draw(sprites, ortho);
		this->state = BlockState::Ready;
	}
	void draw(OrderedImageRgbaU8& diffuseTarget, OrderedImageRgbaU8& normalTarget, AlignedImageF32& heightTarget, const IRect& seenRegion) const {
		if (this->state != BlockState::Unused) {
			int left = this->worldRegion.left() - seenRegion.left();
			int top = this->worldRegion.top() - seenRegion.top();
			draw_copy(diffuseTarget, this->diffuseBuffer, left, top);
			draw_copy(normalTarget, this->normalBuffer, left, top);
			draw_copy(heightTarget, this->heightBuffer, left, top);
		}
	}
	void recycle() {
		//printText("Recycle block at ", this->worldRegion, "\n");
		this->state = BlockState::Unused;
		this->worldRegion = IRect();
		this->cameraId = -1;
	}
};

// TODO: A way to delete passive sprites using search criterias for bounding box and leaf content using a boolean lambda
class SpriteWorldImpl {
public:
	// World
	OrthoSystem ortho;
	// Sprites that rarely change and can be stored in a background image. (no animations allowed)
	// TODO: Don't store the position twice, by keeping it separate from the Sprite struct.
	Octree<Sprite> passiveSprites;
	// Temporary things are deleted when spriteWorld_clearTemporary is called
	List<Sprite> temporarySprites;
	List<PointLight> temporaryPointLights;
	List<DirectedLight> temporaryDirectedLights;
	// View
	int cameraIndex = 0;
	IVector3D cameraLocation;
	// Deferred rendering
	OrderedImageRgbaU8 diffuseBuffer;
	OrderedImageRgbaU8 normalBuffer;
	AlignedImageF32 heightBuffer;
	OrderedImageRgbaU8 lightBuffer;
	// Passive background
	// TODO: How can split-screen use multiple cameras without duplicate blocks or deleting the other camera's blocks by distance?
	List<BackgroundBlock> backgroundBlocks;
private:
	// Reused buffers
	int shadowResolution;
	CubeMapF32 temporaryShadowMap;
public:
	SpriteWorldImpl(const OrthoSystem &ortho, int shadowResolution)
	: ortho(ortho), passiveSprites(ortho_miniUnitsPerTile * 64), shadowResolution(shadowResolution), temporaryShadowMap(shadowResolution) {}
public:
	void updateBlockAt(const IRect& blockRegion, const IRect& seenRegion) {
		int unusedBlockIndex = -1;
		// Find an existing block
		for (int b = 0; b < this->backgroundBlocks.length(); b++) {
			BackgroundBlock* currentBlockPtr = &this->backgroundBlocks[b];
			if (currentBlockPtr->state != BlockState::Unused) {
				// Check direction
				if (currentBlockPtr->cameraId == this->ortho.view[this->cameraIndex].id) {
					// Check location
					if (currentBlockPtr->worldRegion.left() == blockRegion.left() && currentBlockPtr->worldRegion.top() == blockRegion.top()) {
						// Update if needed
						if (currentBlockPtr->state == BlockState::Dirty) {
							currentBlockPtr->update(this->passiveSprites, blockRegion, this->ortho.view[this->cameraIndex]);
						}
						// Use the block
						return;
					} else {
						// See if the block is too far from the camera
						if (currentBlockPtr->worldRegion.right() < seenRegion.left() - BackgroundBlock::maxDistance
						 || currentBlockPtr->worldRegion.left() > seenRegion.right() + BackgroundBlock::maxDistance
						 || currentBlockPtr->worldRegion.bottom() < seenRegion.top() - BackgroundBlock::maxDistance
					 	 || currentBlockPtr->worldRegion.top() > seenRegion.bottom() + BackgroundBlock::maxDistance) {
							// Recycle because it's too far away
							currentBlockPtr->recycle();
							unusedBlockIndex = b;
						}
					}
				} else{
					// Recycle directly when another camera angle is used
					currentBlockPtr->recycle();
					unusedBlockIndex = b;
				}
			} else {
				unusedBlockIndex = b;
			}
		}
		// If none of them matched, we should've passed by any unused block already
		if (unusedBlockIndex > -1) {
			// We have a block to reuse
			this->backgroundBlocks[unusedBlockIndex].update(this->passiveSprites, blockRegion, this->ortho.view[this->cameraIndex]);
		} else {
			// Create a new block
			this->backgroundBlocks.pushConstruct(this->passiveSprites, blockRegion, this->ortho.view[this->cameraIndex]);
		}
	}
	void invalidateBlockAt(int left, int top) {
		// Find an existing block
		for (int b = 0; b < this->backgroundBlocks.length(); b++) {
			BackgroundBlock* currentBlockPtr = &this->backgroundBlocks[b];
			// Assuming that alternative camera angles will be removed when drawing next time
			if (currentBlockPtr->state == BlockState::Ready
			 && currentBlockPtr->worldRegion.left() == left
			 && currentBlockPtr->worldRegion.top() == top) {
				// Make dirty to force an update
				currentBlockPtr->state = BlockState::Dirty;
			}
		}
	}
	// Make sure that each pixel in seenRegion is occupied by an updated background block
	void updateBlocks(const IRect& seenRegion) {
		// Round inclusive pixel indices down to containing blocks and iterate over them in strides along x and y
		int64_t roundedLeft = roundDown(seenRegion.left(), BackgroundBlock::blockSize);
		int64_t roundedTop = roundDown(seenRegion.top(), BackgroundBlock::blockSize);
		int64_t roundedRight = roundDown(seenRegion.right() - 1, BackgroundBlock::blockSize);
		int64_t roundedBottom = roundDown(seenRegion.bottom() - 1, BackgroundBlock::blockSize);
		for (int64_t y = roundedTop; y <= roundedBottom; y += BackgroundBlock::blockSize) {
			for (int64_t x = roundedLeft; x <= roundedRight; x += BackgroundBlock::blockSize) {
				// Make sure that a block is allocated and pre-drawn at this location
				this->updateBlockAt(IRect(x, y, BackgroundBlock::blockSize, BackgroundBlock::blockSize), seenRegion);
			}
		}
	}
	void drawDeferred(OrderedImageRgbaU8& diffuseTarget, OrderedImageRgbaU8& normalTarget, AlignedImageF32& heightTarget, const IRect& seenRegion) {
		// Check image dimensions
		assert(image_getWidth(diffuseTarget) == seenRegion.width() && image_getHeight(diffuseTarget) == seenRegion.height());
		assert(image_getWidth(normalTarget) == seenRegion.width() && image_getHeight(normalTarget) == seenRegion.height());
		assert(image_getWidth(heightTarget) == seenRegion.width() && image_getHeight(heightTarget) == seenRegion.height());
		// Draw passive sprites to blocks
		this->updateBlocks(seenRegion);
		// Draw blocks to the targets
		for (int b = 0; b < this->backgroundBlocks.length(); b++) {
			this->backgroundBlocks[b].draw(diffuseTarget, normalTarget, heightTarget, seenRegion);
		}
		// Draw active sprites to the targets
		for (int s = 0; s < this->temporarySprites.length(); s++) {
			drawSprite(this->temporarySprites[s], this->ortho.view[this->cameraIndex], -seenRegion.upperLeft(), heightTarget, diffuseTarget, normalTarget);
		}
	}
public:
	void updatePassiveRegion(const IRect& modifiedRegion) {
		int64_t roundedLeft = roundDown(modifiedRegion.left(), BackgroundBlock::blockSize);
		int64_t roundedTop = roundDown(modifiedRegion.top(), BackgroundBlock::blockSize);
		int64_t roundedRight = roundDown(modifiedRegion.right() - 1, BackgroundBlock::blockSize);
		int64_t roundedBottom = roundDown(modifiedRegion.bottom() - 1, BackgroundBlock::blockSize);
		for (int64_t y = roundedTop; y <= roundedBottom; y += BackgroundBlock::blockSize) {
			for (int64_t x = roundedLeft; x <= roundedRight; x += BackgroundBlock::blockSize) {
				// Make sure that a block is allocated and pre-drawn at this location
				this->invalidateBlockAt(x, y);
			}
		}
	}
	IVector2D findWorldCenter(const AlignedImageRgbaU8& colorTarget) const {
		return IVector2D(image_getWidth(colorTarget) / 2, image_getHeight(colorTarget) / 2) - this->ortho.miniTileOffsetToScreenPixel(this->cameraLocation, this->cameraIndex);
	}
	void draw(AlignedImageRgbaU8& colorTarget) {
		double startTime;

		IVector2D worldCenter = this->findWorldCenter(colorTarget);

		// Resize when the window has resized or the buffers haven't been allocated before
		int width = image_getWidth(colorTarget);
		int height = image_getHeight(colorTarget);
		if (image_getWidth(this->diffuseBuffer) != width || image_getHeight(this->diffuseBuffer) != height) {
			this->diffuseBuffer = image_create_RgbaU8(width, height);
			this->normalBuffer = image_create_RgbaU8(width, height);
			this->lightBuffer = image_create_RgbaU8(width, height);
			this->heightBuffer = image_create_F32(width, height);
		}

		IRect worldRegion = IRect(-worldCenter.x, -worldCenter.y, width, height);
		startTime = time_getSeconds();
			this->drawDeferred(this->diffuseBuffer, this->normalBuffer, this->heightBuffer, worldRegion);
		debugText("Draw deferred: ", (time_getSeconds() - startTime) * 1000.0, " ms\n");

		// Illuminate using directed lights
		if (this->temporaryDirectedLights.length() > 0) {
			startTime = time_getSeconds();
				// Overwriting any light from the previous frame
				for (int p = 0; p < this->temporaryDirectedLights.length(); p++) {
					this->temporaryDirectedLights[p].illuminate(this->ortho.view[this->cameraIndex], worldCenter, this->lightBuffer, this->normalBuffer, p == 0);
				}
			debugText("Sun light: ", (time_getSeconds() - startTime) * 1000.0, " ms\n");
		} else {
			startTime = time_getSeconds();
				image_fill(this->lightBuffer, ColorRgbaI32(0)); // Set light to black
			debugText("Clear light: ", (time_getSeconds() - startTime) * 1000.0, " ms\n");
		}

		// Illuminate using point lights
		for (int p = 0; p < this->temporaryPointLights.length(); p++) {
			PointLight *currentLight = &this->temporaryPointLights[p];
			if (currentLight->shadowCasting) {
				startTime = time_getSeconds();
				this->temporaryShadowMap.clear();
				currentLight->renderSpriteShadows(this->temporaryShadowMap, this->passiveSprites, ortho.view[this->cameraIndex].normalToWorldSpace);
				for (int s = 0; s < this->temporarySprites.length(); s++) {
					currentLight->renderSpriteShadow(this->temporaryShadowMap, this->temporarySprites[s], ortho.view[this->cameraIndex].normalToWorldSpace);
				}
				debugText("Cast point-light shadows: ", (time_getSeconds() - startTime) * 1000.0, " ms\n");
			}
			startTime = time_getSeconds();
			currentLight->illuminate(this->ortho.view[this->cameraIndex], worldCenter, this->lightBuffer, this->normalBuffer, this->heightBuffer, this->temporaryShadowMap);
			debugText("Illuminate from point-light: ", (time_getSeconds() - startTime) * 1000.0, " ms\n");
		}

		// Draw the final image to the target by multiplying diffuse with light
		startTime = time_getSeconds();
			blendLight(colorTarget, this->diffuseBuffer, this->lightBuffer);
		debugText("Blend light: ", (time_getSeconds() - startTime) * 1000.0, " ms\n");
	}
};

int sprite_loadTypeFromFile(const String& folderPath, const String& spriteName) {
	types.pushConstruct(folderPath, spriteName);
	return types.length() - 1;
}

int sprite_getTypeCount() {
	return types.length();
}

SpriteWorld spriteWorld_create(OrthoSystem ortho, int shadowResolution) {
	return std::make_shared<SpriteWorldImpl>(ortho, shadowResolution);
}

#define MUST_EXIST(OBJECT, METHOD) if (OBJECT.get() == nullptr) { throwError("The " #OBJECT " handle was null in " #METHOD "\n"); }

void spriteWorld_addBackgroundSprite(SpriteWorld& world, const Sprite& sprite) {
	MUST_EXIST(world, spriteWorld_addBackgroundSprite);
	// TODO: Validate type index before looking up the bounding box, for easy debugging
	// TODO: Replace sprite.location with a separate position argument, possibly constructing in place using the API
	// Add the passive sprite to the octree
	IVector3D origin = sprite.location;
	IVector3D minBound = origin + types[sprite.typeIndex].minBoundMini;
	IVector3D maxBound = origin + types[sprite.typeIndex].maxBoundMini;
	world->passiveSprites.insert(sprite, origin, minBound, maxBound);
	// Find the affected passive region and make it dirty
	int frameIndex = getSpriteFrameIndex(sprite, world->ortho.view[world->cameraIndex]);
	const SpriteFrame* frame = &types[sprite.typeIndex].frames[frameIndex];
	IVector2D upperLeft = world->ortho.miniTilePositionToScreenPixel(sprite.location, world->cameraIndex, IVector2D()) - frame->centerPoint;
	IRect region = IRect(upperLeft.x, upperLeft.y, image_getWidth(frame->colorImage), image_getHeight(frame->colorImage));
	world->updatePassiveRegion(region);
}

void spriteWorld_addTemporarySprite(SpriteWorld& world, const Sprite& sprite) {
	MUST_EXIST(world, spriteWorld_addTemporarySprite);
	// Add the temporary sprite
	world->temporarySprites.push(sprite);
}

void spriteWorld_createTemporary_pointLight(SpriteWorld& world, const FVector3D position, float radius, float intensity, ColorRgbI32 color, bool shadowCasting) {
	MUST_EXIST(world, spriteWorld_createTemporary_pointLight);
	world->temporaryPointLights.pushConstruct(position, radius, intensity, color, shadowCasting);
}

void spriteWorld_createTemporary_directedLight(SpriteWorld& world, const FVector3D direction, float intensity, ColorRgbI32 color) {
	MUST_EXIST(world, spriteWorld_createTemporary_pointLight);
	world->temporaryDirectedLights.pushConstruct(direction, intensity, color);
}

void spriteWorld_clearTemporary(SpriteWorld& world) {
	MUST_EXIST(world, spriteWorld_clearTemporary);
	world->temporarySprites.clear();
	world->temporaryPointLights.clear();
	world->temporaryDirectedLights.clear();
}

void spriteWorld_draw(SpriteWorld& world, AlignedImageRgbaU8& colorTarget) {
	MUST_EXIST(world, spriteWorld_draw);
	world->draw(colorTarget);
}

IVector3D spriteWorld_findGroundAtPixel(SpriteWorld& world, const AlignedImageRgbaU8& colorBuffer, const IVector2D& pixelLocation) {
	MUST_EXIST(world, spriteWorld_findGroundAtPixel);
	return world->ortho.pixelToMiniPosition(pixelLocation, world->cameraIndex, world->findWorldCenter(colorBuffer));
}

void spriteWorld_moveCameraInPixels(SpriteWorld& world, const IVector2D& pixelOffset) {
	MUST_EXIST(world, spriteWorld_moveCameraInPixels);
	world->cameraLocation = world->cameraLocation + world->ortho.pixelToMiniOffset(pixelOffset, world->cameraIndex);
}

AlignedImageRgbaU8 spriteWorld_getDiffuseBuffer(SpriteWorld& world) {
	MUST_EXIST(world, spriteWorld_getDiffuseBuffer);
	return world->diffuseBuffer;
}

OrderedImageRgbaU8 spriteWorld_getNormalBuffer(SpriteWorld& world) {
	MUST_EXIST(world, spriteWorld_getNormalBuffer);
	return world->normalBuffer;
}

OrderedImageRgbaU8 spriteWorld_getLightBuffer(SpriteWorld& world) {
	MUST_EXIST(world, spriteWorld_getLightBuffer);
	return world->lightBuffer;
}

AlignedImageF32 spriteWorld_getHeightBuffer(SpriteWorld& world) {
	MUST_EXIST(world, spriteWorld_getHeightBuffer);
	return world->heightBuffer;
}

int spriteWorld_getCameraDirectionIndex(SpriteWorld& world) {
	MUST_EXIST(world, spriteWorld_getCameraDirectionIndex);
	return world->cameraIndex;
}

void spriteWorld_setCameraDirectionIndex(SpriteWorld& world, int index) {
	MUST_EXIST(world, spriteWorld_setCameraDirectionIndex);
	world->cameraIndex = index;
}

static FVector3D normalFromPoints(const FVector3D& A, const FVector3D& B, const FVector3D& C) {
    return normalize(crossProduct(B - A, C - A));
}

static FVector3D getAverageNormal(const Model& model, int part, int poly) {
	int vertexCount = model_getPolygonVertexCount(model, part, poly);
	FVector3D normalSum;
	for (int t = 0; t < vertexCount - 2; t++) {
		normalSum = normalSum + normalFromPoints(
		  model_getVertexPosition(model, part, poly, 0),
		  model_getVertexPosition(model, part, poly, t + 1),
		  model_getVertexPosition(model, part, poly, t + 2)
		);
	}
	return normalize(normalSum);
}

// Pre-conditions:
//   * All images must exist and have the same dimensions
//   * All triangles in model must be contained within the image bounds after being projected using view
// TODO: Render directly with a location to a 16-bit depth buffer for background 3D models and brush preview
static void sprite_render(Model model, OrthoView view, ImageF32 depthBuffer, ImageRgbaU8 diffuseTarget, ImageRgbaU8 normalTarget) {
	int pointCount = model_getNumberOfPoints(model);
	IRect clipBound = image_getBound(depthBuffer);
	FVector2D projectionOffset = FVector2D((float)clipBound.width() * 0.5f, (float)clipBound.height() * 0.5f);
	// TODO: Allow having length 0 for Arrays and Fields by preventing all access to elements in special cases
	Array<FVector3D> projectedPoints(pointCount, FVector3D()); // pixel X, pixel Y, mini-tile height
	Array<FVector3D> normalPoints(pointCount, FVector3D()); // normal X, Y, Z
	// TODO: Store an array of normals for each point, sum normal vectors for each included polygon and normalize the result
	//       Interpolate and normalize again for each pixel
	for (int point = 0; point < pointCount; point++) {
		FVector3D projected = view.worldSpaceToScreenDepth.transform(model_getPoint(model, point));
		projectedPoints[point] = FVector3D(projected.x + projectionOffset.x, projected.y + projectionOffset.y, projected.z);
	}

	// Calculate rounded normals in light-space.
	// TODO: Pre-generate normals in world space before transforming into light space.
	FMatrix3x3 normalToWorldSpace = view.normalToWorldSpace;
	for (int part = 0; part < model_getNumberOfParts(model); part++) {
		for (int poly = 0; poly < model_getNumberOfPolygons(model, part); poly++) {
			// Transform the normal into a coordinate system aligned with the camera.
			//       Otherwise the rotation cannot be used for individual rotation to have a corner for each wall.
			FVector3D worldNormal = getAverageNormal(model, part, poly);
			FVector3D localNormal = normalToWorldSpace.transformTransposed(worldNormal);
			for (int vert = 0; vert < model_getPolygonVertexCount(model, part, poly); vert++) {
				int point = model_getVertexPointIndex(model, part, poly, vert);
				normalPoints[point] = normalPoints[point] + localNormal;
			}
		}
	}
	for (int point = 0; point < pointCount; point++) {
		normalPoints[point] = normalize(normalPoints[point]);
	}

	// Render polygons as triangle fans
	for (int part = 0; part < model_getNumberOfParts(model); part++) {
		for (int poly = 0; poly < model_getNumberOfPolygons(model, part); poly++) {
			int vertexCount = model_getPolygonVertexCount(model, part, poly);
			int vertA = 0;
			FVector4D vertexColorA = model_getVertexColor(model, part, poly, vertA) * 255.0f;
			int indexA = model_getVertexPointIndex(model, part, poly, vertA);
			FVector3D normalA = normalPoints[indexA];
			FVector3D pointA = projectedPoints[indexA];
			LVector2D subPixelA = LVector2D(safeRoundInt64(pointA.x * constants::unitsPerPixel), safeRoundInt64(pointA.y * constants::unitsPerPixel));
			for (int vertB = 1; vertB < vertexCount - 1; vertB++) {
				int vertC = vertB + 1;
				int indexB = model_getVertexPointIndex(model, part, poly, vertB);
				int indexC = model_getVertexPointIndex(model, part, poly, vertC);
				FVector4D vertexColorB = model_getVertexColor(model, part, poly, vertB) * 255.0f;
				FVector4D vertexColorC = model_getVertexColor(model, part, poly, vertC) * 255.0f;
				FVector3D normalB = normalPoints[indexB];
				FVector3D normalC = normalPoints[indexC];
				FVector3D pointB = projectedPoints[indexB];
				FVector3D pointC = projectedPoints[indexC];
				LVector2D subPixelB = LVector2D(safeRoundInt64(pointB.x * constants::unitsPerPixel), safeRoundInt64(pointB.y * constants::unitsPerPixel));
				LVector2D subPixelC = LVector2D(safeRoundInt64(pointC.x * constants::unitsPerPixel), safeRoundInt64(pointC.y * constants::unitsPerPixel));
				IRect triangleBound = IRect::cut(clipBound, getTriangleBound(subPixelA, subPixelB, subPixelC));
				int rowCount = triangleBound.height();
				if (rowCount > 0) {
					// TODO: Fix the excess pixel bugs
					RowInterval rows[rowCount];
					rasterizeTriangle(subPixelA, subPixelB, subPixelC, rows, triangleBound);
					for (int y = triangleBound.top(); y < triangleBound.bottom(); y++) {
						int rowIndex = y - triangleBound.top();
						int left = rows[rowIndex].left;
						int right = rows[rowIndex].right;
						for (int x = left; x < right; x++) {
							FVector3D weight = getAffineWeight(FVector2D(pointA.x, pointA.y), FVector2D(pointB.x, pointB.y), FVector2D(pointC.x, pointC.y), FVector2D(x + 0.5f, y + 0.5f));
							float height = interpolateUsingAffineWeight(pointA.z, pointB.z, pointC.z, weight);
							if (height > image_readPixel_clamp(depthBuffer, x, y)) {
								FVector4D vertexColor = interpolateUsingAffineWeight(vertexColorA, vertexColorB, vertexColorC, weight);
								FVector3D normal = (normalize(interpolateUsingAffineWeight(normalA, normalB, normalC, weight)) + 1.0f) * 127.5f;
								image_writePixel(depthBuffer, x, y, height);
								image_writePixel(diffuseTarget, x, y, ColorRgbaI32(vertexColor.x, vertexColor.y, vertexColor.z, 255));
								image_writePixel(normalTarget, x, y, ColorRgbaI32(normal.x, normal.y, normal.z, 255));
							}
						}
					}
				}
			}
		}
	}
}

void sprite_generateFromModel(ImageRgbaU8& targetAtlas, String& targetConfigText, const Model& visibleModel, const Model& shadowModel, const OrthoSystem& ortho, const String& targetPath, int cameraAngles) {
	// Validate input
	if (cameraAngles < 1) {
		printText("  Need at least one camera angle to generate a sprite!\n");
		return;
	} else if (!model_exists(visibleModel)) {
		printText("  There's nothing to render, because visible model does not exist!\n");
		return;
	} else if (model_getNumberOfParts(visibleModel) == 0) {
		printText("  There's nothing to render in the visible model, because there are no parts in the visible model!\n");
		return;
	} else {
		// Measure the bounding cylinder for determining the uncropped image size
		FVector3D minBound = FVector3D(std::numeric_limits<float>::max());
		FVector3D maxBound = FVector3D(-std::numeric_limits<float>::max());
		for (int p = 0; p < model_getNumberOfPoints(visibleModel); p++) {
			FVector3D point = model_getPoint(visibleModel, p);
			if (point.x < minBound.x) { minBound.x = point.x; }
			if (point.y < minBound.y) { minBound.y = point.y; }
			if (point.z < minBound.z) { minBound.z = point.z; }
			if (point.x > maxBound.x) { maxBound.x = point.x; }
			if (point.y > maxBound.y) { maxBound.y = point.y; }
			if (point.z > maxBound.z) { maxBound.z = point.z; }
		}
		// Check if generating a bound failed
		if (minBound.x > maxBound.x) {
			printText("  There's nothing visible in the model, because the 3D bounding box had no points to be created from!\n");
			return;
		}

		printText("  Representing height from ", minBound.y, " to ", maxBound.y, " encoded using 8-bits\n");

		// Calculate initial image size
		float worstCaseDiameter = (std::max(maxBound.x, -minBound.x) + std::max(maxBound.y, -minBound.y) + std::max(maxBound.z, -minBound.z)) * 2;
		int maxRes = roundUp(worstCaseDiameter * ortho.pixelsPerTile, 2) + 4; // Round up to even pixels and add 4 padding pixels

		// Allocate square images from the pessimistic size estimation
		int width = maxRes;
		int height = maxRes;
		ImageF32 depthBuffer = image_create_F32(width, height);
		ImageRgbaU8 colorImage[cameraAngles];
		ImageRgbaU8 heightImage[cameraAngles];
		ImageRgbaU8 normalImage[cameraAngles];
		for (int a = 0; a < cameraAngles; a++) {
			colorImage[a] = image_create_RgbaU8(width, height);
			heightImage[a] = image_create_RgbaU8(width, height);
			normalImage[a] = image_create_RgbaU8(width, height);
		}
		// Render the model to multiple render targets at once
		float heightScale = 255.0f / (maxBound.y - minBound.y);
		for (int a = 0; a < cameraAngles; a++) {
			image_fill(depthBuffer, -1000000000.0f);
			image_fill(colorImage[a], ColorRgbaI32(0, 0, 0, 0));
			sprite_render(visibleModel, ortho.view[a], depthBuffer, colorImage[a], normalImage[a]);
			// Convert height into an 8 bit channel for saving
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					int32_t opacityPixel = image_readPixel_clamp(colorImage[a], x, y).alpha;
					int32_t heightPixel = (image_readPixel_clamp(depthBuffer, x, y) - minBound.y) * heightScale;
					image_writePixel(heightImage[a], x, y, ColorRgbaI32(heightPixel, 0, 0, opacityPixel));
				}
			}
		}

		// Crop all images uniformly for easy atlas packing
		int32_t minX = width;
		int32_t minY = height;
		int32_t maxX = 0;
		int32_t maxY = 0;
		for (int a = 0; a < cameraAngles; a++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					if (image_readPixel_border(colorImage[a], x, y).alpha) {
						if (x < minX) minX = x;
						if (x > maxX) maxX = x;
						if (y < minY) minY = y;
						if (y > maxY) maxY = y;
					}
				}
			}
		}
		// Check if cropping failed
		if (minX > maxX) {
			printText("  There's nothing visible in the model, because cropping the final images returned nothing!\n");
			return;
		}

		IRect cropRegion = IRect(minX, minY, (maxX + 1) - minX, (maxY + 1) - minY);
		if (cropRegion.width() < 1 || cropRegion.height() < 1) {
			printText("  Cropping failed to find any drawn pixels!\n");
			return;
		}
		for (int a = 0; a < cameraAngles; a++) {
			colorImage[a] = image_getSubImage(colorImage[a], cropRegion);
			heightImage[a] = image_getSubImage(heightImage[a], cropRegion);
			normalImage[a] = image_getSubImage(normalImage[a], cropRegion);
		}
		int croppedWidth = cropRegion.width();
		int croppedHeight = cropRegion.height();
		int centerX = width / 2 - cropRegion.left();
		int centerY = height / 2 - cropRegion.top();
		printText("  Cropped images of ", croppedWidth, "x", croppedHeight, " pixels with centers at (", centerX, ", ", centerY, ")\n");

		// Pack everything into an image atlas
		targetAtlas = image_create_RgbaU8(croppedWidth * 3, croppedHeight * cameraAngles);
		for (int a = 0; a < cameraAngles; a++) {
			draw_copy(targetAtlas, colorImage[a], 0, a * croppedHeight);
			draw_copy(targetAtlas, heightImage[a], croppedWidth, a * croppedHeight);
			draw_copy(targetAtlas, normalImage[a], croppedWidth * 2, a * croppedHeight);
		}

		SpriteConfig config = SpriteConfig(centerX, centerY, cameraAngles, 3, minBound, maxBound);
		if (model_exists(shadowModel) && model_getNumberOfPoints(shadowModel) > 0) {
			config.appendShadow(shadowModel);
		}
		targetConfigText = config.toIni();
	}
}

// Allowing the last decimals to deviate a bit because floating-point operations are rounded differently between computers
static bool approximateTextMatch(const ReadableString &a, const ReadableString &b, double tolerance = 0.00002) {
	int readerA = 0, readerB = 0;
	while (readerA < string_length(a) && readerB < string_length(b)) {
		DsrChar charA = a[readerA];
		DsrChar charB = b[readerB];
		if (character_isValueCharacter(charA) && character_isValueCharacter(charB)) {
			// Scan forward on both sides while consuming content and comparing the actual value
			int startA = readerA;
			int startB = readerB;
			// Only move forward on valid characters
			if (a[readerA] == U'-') { readerA++; }
			if (b[readerB] == U'-') { readerB++; }
			while (character_isDigit(a[readerA])) { readerA++; }
			while (character_isDigit(b[readerB])) { readerB++; }
			if (a[readerA] == U'.') { readerA++; }
			if (b[readerB] == U'.') { readerB++; }
			while (character_isDigit(a[readerA])) { readerA++; }
			while (character_isDigit(b[readerB])) { readerB++; }
			// Approximate values
			double valueA = string_toDouble(string_exclusiveRange(a, startA, readerA));
			double valueB = string_toDouble(string_exclusiveRange(b, startB, readerB));
			// Check the difference
			double diff = valueB - valueA;
			if (diff > tolerance || diff < -tolerance) {
				// Too big difference, this is probably not a rounding error
				return false;
			}
		} else if (charA != charB) {
			// Difference with a non-value involved
			return false;
		}
		readerA++;
		readerB++;
	}
	if (readerA < string_length(a) - 1 || readerB < string_length(b) - 1) {
		// One text had unmatched remains after the other reached its end
		return false;
	} else {
		return true;
	}
}

void sprite_generateFromModel(const Model& visibleModel, const Model& shadowModel, const OrthoSystem& ortho, const String& targetPath, int cameraAngles, bool debug) {
	// Generate an image and a configuration file from the visible model
	ImageRgbaU8 atlasImage; String configText;
	sprite_generateFromModel(atlasImage, configText, visibleModel, shadowModel, ortho, targetPath, cameraAngles);
	// Save the result on success
	if (string_length(configText) > 0) {
		// Save the atlas
		String atlasPath = targetPath + U".png";
		// Try loading any existing image
		ImageRgbaU8 existingAtlasImage = image_load_RgbaU8(atlasPath, false);
		if (image_exists(existingAtlasImage)) {
			int difference = image_maxDifference(atlasImage, existingAtlasImage);
			if (difference <= 2) {
				printText("  No significant changes against ", targetPath, ".\n");
			} else {
				image_save(atlasImage, atlasPath);
				printText("  Updated ", targetPath, " with a deviation of ", difference, ".\n");
			}
		} else {
			// Only save if there was no existing image or it differed significantly from the new result
			// This comparison is made to avoid flooding version history with changes from invisible differences in color rounding
			image_save(atlasImage, atlasPath);
			printText("  Saved atlas to ", targetPath, ".\n");
		}

		// Save the configuration
		String configPath = targetPath + U".ini";
		String oldConfixText = string_load(configPath, false);
		if (approximateTextMatch(configText, oldConfixText)) {
			printText("  No significant changes against ", targetPath, ".\n\n");
		} else {
			string_save(targetPath + U".ini", configText);
			printText("  Saved sprite config to ", targetPath, ".\n\n");
		}

		if (debug) {
			ImageRgbaU8 debugImage; String garbageText;
			// TODO: Show overlap between visible and shadow so that shadow outside of visible is displayed as bright red on a dark model.
			//       The number of visible shadow pixels should be reported automatically
			//       in an error message at the end of the total execution together with file names.
			sprite_generateFromModel(debugImage, garbageText, shadowModel, Model(), ortho, targetPath + U"Debug", 8);
			image_save(debugImage, targetPath + U"Debug.png");
		}
	}
}

}

