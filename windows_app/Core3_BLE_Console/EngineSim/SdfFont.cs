using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Raylib_cs;

using Windows.ApplicationModel.Activation;

namespace EngineSim {
	unsafe class SdfFont {
		static bool ShaderValid = false;
		static Shader SDFShader;

		Font Fnt;
		int CurFontSize;

		public int FontSize {
			get {
				return CurFontSize;
			}

			set {
				CurFontSize = value;
			}
		}

		public SdfFont(string FontName, int FontSize, int GlyphCount = 95) {
			Image FntImg;

			CurFontSize = FontSize;

			Fnt = new Font();
			Fnt.BaseSize = FontSize;
			Fnt.GlyphCount = GlyphCount;

			byte[] FontData = File.ReadAllBytes(FontName);
			fixed (byte* FontDataPtr = FontData) {
				Fnt.Glyphs = Raylib.LoadFontData(FontDataPtr, FontData.Length, Fnt.BaseSize, null, 0, FontType.Sdf);
			}

			fixed (Rectangle** FntRecsPtr = &Fnt.Recs) {
				FntImg = Raylib.GenImageFontAtlas(Fnt.Glyphs, &(*FntRecsPtr), Fnt.GlyphCount, Fnt.BaseSize, 0, 1);
				Fnt.Texture = Raylib.LoadTextureFromImage(FntImg);

				Raylib.SetTextureFilter(Fnt.Texture, TextureFilter.Bilinear);
			}

			if (!ShaderValid) {
				ShaderValid = true;
				SDFShader = Raylib.LoadShader(null, "data/shaders/sdf.frag");
			}
		}

		public Vector2 MeasureText(string Txt, float Spacing) {
			return Raylib.MeasureTextEx(Fnt, Txt, FontSize, Spacing);
		}

		public void DrawTextEx(string Txt, Vector2 Pos, float Spacing, Color Tint) {
			Raylib.BeginShaderMode(SDFShader);
			Raylib.DrawTextEx(Fnt, Txt, Pos, FontSize, Spacing, Tint);
			Raylib.EndShaderMode();
		}

		public void DrawTextPro(string Txt, Vector2 Pos, Vector2 Orig, float Rot, float Spacing, Color Tint) {
			Raylib.BeginShaderMode(SDFShader);
			Raylib.DrawTextPro(Fnt, Txt, Pos, Orig, Rot, FontSize, Spacing, Tint);
			Raylib.EndShaderMode();
		}
	}
}
